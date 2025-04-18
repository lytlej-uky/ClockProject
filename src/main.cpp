#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include "clock.hpp"

SemaphoreHandle_t xMutex;
const int kInterruptPin = 15;

void setup() {
  Serial.begin(9600);
  xMutex = xSemaphoreCreateMutex();
  
  Serial.println("Starting clock task...");
  
  time_tracker::beginTasks();

  pinMode(kInterruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(kInterruptPin), time_tracker::handleInterrupt, FALLING);

  // vTaskStartScheduler();
}

void loop() {

}
