ESP32 Code to Open my Gate with a Solenoid
------------------------------------------

The code resides in [main/knocker.c](main/knocker.c).

A microphone with a high pass filter is connected to GPIO_INPUT_MIC. A contact
button is connected to GPIO_INPUT_BUTTON. Tapping a pattern on the microphone
unlocks the door by sending a pulse GPIO_OUTPUT_IO_0, which is hooked up to a
power transistor that drives a solenoid. Pushing the button also unlatches the
solenoid in the same way.
