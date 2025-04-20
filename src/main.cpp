#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include "clock.hpp"

SemaphoreHandle_t xMutex;
const int kInterruptPinStartTimer = 15;
const int kInterruptPinIncMins = 18;
const int kInterruptPinIncHrs = 19;

void setup() {
  Serial.begin(9600);
  xMutex = xSemaphoreCreateMutex();
  
  Serial.println("Starting clock task...");
  
  time_tracker::beginTasks();

  pinMode(kInterruptPinStartTimer, INPUT);
  pinMode(kInterruptPinIncMins, INPUT);
  pinMode(kInterruptPinIncHrs, INPUT);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinStartTimer), time_tracker::handleInterruptStartTimer, FALLING);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinIncMins), time_tracker::handleInterruptIncMins, FALLING);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinIncHrs), time_tracker::handleInterruptIncHrs, FALLING);

  // vTaskStartScheduler();
}

void loop() {

}
