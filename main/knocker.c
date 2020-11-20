#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_INPUT_BUTTON     0
#define GPIO_INPUT_MIC     5
#define ESP_INTR_FLAG_DEFAULT 0

typedef enum { EV_PUSH } Event;

static xQueueHandle microphone_event_queue = NULL;
static xQueueHandle button_event_queue = NULL;


static void unlock() {
    printf("Unlocking\n");
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    vTaskDelay(2000 / portTICK_RATE_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    printf("Done unlock\n");
}


static TickType_t last_isr_tick;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    const int pin = (int)arg;

    TickType_t t = xTaskGetTickCountFromISR();

    // Only recognize this as an event if there was plenty of delay since the last event.
    if(t - last_isr_tick > 20 / portTICK_PERIOD_MS) {
      Event event = EV_PUSH;
      xQueueSendFromISR(pin == GPIO_INPUT_BUTTON ? button_event_queue :  microphone_event_queue, &event, NULL);
    }
    last_isr_tick = t;
}


static void microphone_task(void* arg)
{
    const int MAX_CODE_LENGTH = 10;
    char code[MAX_CODE_LENGTH + 1];
    int code_index = 0;
    int num_timeouts = 0;

    for(;;) {
        Event event;

        if(!xQueueReceive(microphone_event_queue, &event, portMAX_DELAY))
            continue; // Timeout waiting for a button. Try again.

        code_index = 0;
        memset(code, '\0', MAX_CODE_LENGTH + 1);
        code[code_index] = '1';
        num_timeouts = 0;

        // Start capturing the code.
        for(;;) {
            printf("Code so far: '%s'\n", code);

            // Wait for a button.
            if(xQueueReceive(microphone_event_queue, &event, 800 / portTICK_PERIOD_MS)) {
                printf("Button push!\n");

                code[code_index]++;
                num_timeouts = 0;
            } else {
                printf("Timeout\n");

                if(num_timeouts++ > 3) {
                    // This is the end of the code sequence. Reset and wait for a new sequence.
                    break;
                }

                const char expected_code[] = "2230";
                int code_start = strlen(code) - strlen(expected_code);
                if(code_start < 0) code_start = 0;
                if(!strcmp(code + code_start, expected_code)) {
                    printf("Codes match: %s and %s\n", code, expected_code);
                    unlock();
                    break;
                }

                // After a timeout, start accumulating the count for the next code. But
                // only do so if this code has a non-zero count. This way, we never get
                // 0 counts in the code string.
                if(code[code_index] != '0' && code_index < MAX_CODE_LENGTH) {
                    code_index++;
                    code[code_index] = '0';
                }
            }
        }
    }
}

static void button_task(void* arg)
{
    for(;;) {
        Event event;

        printf("button top\n");

        xQueueReset(button_event_queue);
        if(!xQueueReceive(button_event_queue, &event, portMAX_DELAY))
            continue; // Timeout waiting for a button. Try again.

        unlock();
    }
}


void app_main(void)
{
    gpio_config_t io_conf;

    io_conf.pin_bit_mask = (1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1);
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = 1ULL<<GPIO_INPUT_BUTTON;
    //interrupt on rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = 1ULL<<GPIO_INPUT_MIC;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // create event queues and tasks.
    microphone_event_queue = xQueueCreate(10, sizeof(Event));
    xTaskCreate(microphone_task, "microphone_task", 2048, NULL, 10, NULL);

    button_event_queue = xQueueCreate(10, sizeof(Event));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);


    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for each pin
    gpio_isr_handler_add(GPIO_INPUT_MIC, gpio_isr_handler, (void*) GPIO_INPUT_MIC);
    gpio_isr_handler_add(GPIO_INPUT_BUTTON, gpio_isr_handler, (void*) GPIO_INPUT_BUTTON);

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    while(1) {
        printf("Heartbeat\n");
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
    //vTaskSuspend(NULL);
}
