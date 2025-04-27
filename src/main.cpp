#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include "clock.hpp"
#include <MD_Parola.h>
#include <MD_MAX72XX.h>

const int kInterruptPinStartTimer = 15;
const int kInterruptPinIncMins = 5;
const int kInterruptPinIncHrs = 19;
const int kCS = 21;
const int kMaxDevices = 4;

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

MD_Parola ledMatrix = MD_Parola(HARDWARE_TYPE, kCS, kMaxDevices);

void setup() {
  Serial.begin(9600);  
  
  Serial.println("Starting clock task...");
  
  time_tracker::beginTasks();

  pinMode(kInterruptPinStartTimer, INPUT);
  pinMode(kInterruptPinIncMins, INPUT);
  pinMode(kInterruptPinIncHrs, INPUT);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinStartTimer), time_tracker::handleInterruptStartTimer, FALLING);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinIncMins), time_tracker::handleInterruptIncMins, FALLING);
  attachInterrupt(digitalPinToInterrupt(kInterruptPinIncHrs), time_tracker::handleInterruptIncHrs, FALLING);

  ledMatrix.begin();         // initialize the object 
  ledMatrix.setIntensity(2); // set the brightness of the LED matrix display (from 0 to 15)
  ledMatrix.displayClear();  // clear led matrix display
}

void loop() {
}
