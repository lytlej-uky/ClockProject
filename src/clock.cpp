#include "clock.hpp"
#include <Arduino.h>

namespace time{

std::tm current_time = {};
SemaphoreHandle_t xMutex;

std::tm getCurrentTime()
{
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time = *std::localtime(&now_time);
  return local_time;
}

void prvUpdateTime(void* pvParameters)
{
  auto clock_task = static_cast<TaskHandle_t*>(pvParameters);
  while (1) {
    auto time_now = getCurrentTime();
    if (time_now.tm_hour != current_time.tm_hour || time_now.tm_min != current_time.tm_min || time_now.tm_sec != current_time.tm_sec) {
      // need to update time!!
      Serial.printf("updating time to %d:%d:%d\n", time_now.tm_hour, time_now.tm_min, time_now.tm_sec);
      xSemaphoreTake(xMutex, portMAX_DELAY);
      current_time = time_now;
      xSemaphoreGive(xMutex);
      xTaskNotify(*clock_task, kTimeUpdate, eNoAction);
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1s
  }
}

void prvClockMain(void* pvParameters) {
  uint32_t notificationValue;
  std::tm cur_time;
  while (1) {
    // Wait for a notification indefinitely
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY) == pdTRUE) {
      // Process the notification
      if (notificationValue & kTimeUpdate) {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        cur_time = current_time;
        xSemaphoreGive(xMutex);
        Serial.printf("Time updated! New time is %d:%d:%d\n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
      }
    }
  }
}

} // namespace time
