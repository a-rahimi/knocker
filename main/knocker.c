#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_IO_1     5
#define ESP_INTR_FLAG_DEFAULT 0

typedef enum { EV_PUSH } Event;

static xQueueHandle gpio_evt_queue = NULL;


static void unlock() {
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    vTaskDelay(2000 / portTICK_RATE_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
}


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    const uint32_t gpio_num = (uint32_t) arg; // not used.

    Event event = EV_PUSH;

    xQueueSendFromISR(gpio_evt_queue, &event, NULL);
}


static void gpio_task_example(void* arg)
{
    const int NUM_CODES = 10;
    char code[NUM_CODES + 1];
    int code_index = 0;
    int num_timeouts = 0;

    for(;;) {
        Event event;

        if(!xQueueReceive(gpio_evt_queue, &event, portMAX_DELAY))
            continue; // Timeout waiting for a button. Try again.

        code_index = 0;
        memset(code, '\0', NUM_CODES + 1);
        code[code_index] = '1';
        num_timeouts = 0;

        // Start capturing the code.
        for(;;) {
            printf("Code so far: '%s'\n", code);

            // Wait for a button.
            if(xQueueReceive(gpio_evt_queue, &event, 800 / portTICK_PERIOD_MS)) {
                printf("Button push!\n");

                code[code_index]++;
                num_timeouts = 0;
            } else {
                printf("Timeout\n");

                if(num_timeouts++ > 3 || code_index >= NUM_CODES) {
                    // This is the end of the code sequence.

                    if(strcmp(code, "2230")) {
                        printf("Codes don't match\n");
                    } else {
                        printf("Unlocking\n");
                        unlock();
                        printf("Done unlock\n");
                    }
     
                    break; // Go back to outer loop that waits for the beginning of the code.
                }

                // After a timeout, start accumulating the count for the next code. But
                // only do so if this code has a non-zero count. This way, we never get
                // 0 counts in the code string.
                if(code[code_index] != '0') {
                    code_index++;
                    code[code_index] = '0';
                }
            }
        }
    }
}


void app_main(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins
    io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1);
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(Event));

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    while(1) {
        printf("Heartbeat");
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
    //vTaskSuspend(NULL);
}
