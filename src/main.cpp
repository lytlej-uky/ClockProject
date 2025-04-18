#include <Arduino.h>
#include "clock.hpp"

void setup() {
  Serial.begin(9600);
  time::xMutex = xSemaphoreCreateMutex();

  TaskHandle_t clock_task;
  xTaskCreate(
    time::prvClockMain,   // Task function
    "ClockMain",     // Name of the task
    configMINIMAL_STACK_SIZE,                // Stack size (in words, not bytes)
    NULL,     // Task input parameter
    2,                   // Priority of the task
    &clock_task                 // Task handle
  );  

  xTaskCreate(
    time::prvUpdateTime,   // Task function
    "UpdateTime",     // Name of the task
    configMINIMAL_STACK_SIZE,                // Stack size (in words, not bytes)
    &clock_task,     // Task input parameter
    3,                   // Priority of the task
    NULL                 // Task handle
  );
  vTaskStartScheduler();
}

void loop() {

}
