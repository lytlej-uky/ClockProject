#include "clock.hpp"
#include <Arduino.h>

extern SemaphoreHandle_t xMutex;

namespace time_tracker{

std::tm current_time = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // Initialize current_time to zero
TaskHandle_t clock_task = NULL;
unsigned long last_interrupt_time = 0;
const unsigned long debounce_delay = 250;
int timerMinutes = 0;
int timerHours = 0;

std::tm getCurrentTime()
{
  time_t now;
  time(&now);
  struct tm local_time;
  localtime_r(&now, &local_time);
  return local_time;
}

void prvUpdateTime(void* pvParameters)
{
  if (xMutex == NULL) {
    Serial.println("xMutex is NULL, cannot proceed.");
    vTaskDelete(NULL); // Exit the task if mutex is not initialized
  }
  if (clock_task == NULL) {
    Serial.println("clock_task is NULL, cannot proceed.");
    vTaskDelete(NULL); // Exit the task if clock_task is not initialized
  }

  std::tm new_time;
  while (1) {
    Serial.println("Checking time...");
    new_time = getCurrentTime();

    if (new_time.tm_hour != current_time.tm_hour ||
        new_time.tm_min != current_time.tm_min ||
        new_time.tm_sec != current_time.tm_sec) {
      // need to update time!!
      Serial.printf("updating time to %d:%d:%d\n", new_time.tm_hour, new_time.tm_min, new_time.tm_sec);
      xSemaphoreTake(xMutex, portMAX_DELAY);
      Serial.println("Got mutex, updating time...");
      current_time = new_time;
      Serial.printf("Updated time to %d:%d:%d\n", new_time.tm_hour, new_time.tm_min, new_time.tm_sec);
      xSemaphoreGive(xMutex);
      Serial.println("Giving mutex, notifying clock task...");
      xTaskNotify(clock_task, kTimeUpdate, eSetBits);
      Serial.println("Notified clock task.");
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1s
  }
}

void prvClockMain(void* pvParameters) {
  uint32_t notificationValue;
  std::tm cur_time;
  while (1) {
    // Wait for a notification indefinitely
    Serial.println("Waiting for notification...");
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY) == pdTRUE) {
      // Process the notification
      Serial.printf("Notification received: %d\n", notificationValue);
      if (notificationValue & kTimeUpdate) {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        cur_time = current_time;
        xSemaphoreGive(xMutex);
        Serial.printf("Time updated! New time is %d:%d:%d\n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
      }
      if (notificationValue & kAlarmInit) {
        Serial.println("Alarm init");
      }
      if (notificationValue & kAlarmIncMin) {
        Serial.println("Minutes ++");
      }
      if (notificationValue & kAlarmIncHr) {
        Serial.println("Hours ++");
      }
      if (notificationValue & kAlarmEnd) {
        Serial.println("ALARM!!!!!!!!!!!!!!!");
      }
    }
  }
}

void beginTasks() {
  xTaskCreate(
    time_tracker::prvClockMain,   // Task function
    "ClockMain",     // Name of the task
    2048,                // Stack size (in words, not bytes)
    NULL,     // Task input parameter
    2,                   // Priority of the task
    &clock_task                 // Task handle
  );  


  Serial.println("Starting time task...");
  xTaskCreate(
    time_tracker::prvUpdateTime,   // Task function
    "UpdateTime",     // Name of the task
    2048,                // Stack size (in words, not bytes)
    NULL,     // Task input parameter
    3,                   // Priority of the task
    NULL                 // Task handle
  );
}

void IRAM_ATTR handleInterruptStartTimer() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > debounce_delay) {
    last_interrupt_time = current_time;

    // Perform your interrupt action or notify a task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(clock_task, kAlarmInit, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void IRAM_ATTR handleInterruptIncMins() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > debounce_delay) {
    last_interrupt_time = current_time;

    // Perform your interrupt action or notify a task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(clock_task, kAlarmIncMin, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void IRAM_ATTR handleInterruptIncHrs() { 
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > debounce_delay) {
    last_interrupt_time = current_time;

    // Perform your interrupt action or notify a task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(clock_task, kAlarmIncHr, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

} // namespace time_tracker
