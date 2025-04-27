#include "clock.hpp"
#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <sstream>

SemaphoreHandle_t clockMutex;
SemaphoreHandle_t displayMutex;
extern MD_Parola ledMatrix;

namespace time_tracker{

std::tm current_time = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // Initialize current_time to zero
TaskHandle_t clock_task = NULL;
TaskHandle_t display_task = NULL;
unsigned long last_interrupt_time = 0;
const unsigned long debounce_delay = 250;
int timerMinutes = 0;
int timerHours = 0;
int min_adjust = 0;
int hour_adjust = 0;
std::string display_data;
bool colon = false;

std::tm getCurrentTime()
{

  time_t now;
  time(&now);
  struct tm local_time;
  localtime_r(&now, &local_time);
  int carry = 0;
  if (local_time.tm_min + min_adjust >= 60) {
    carry = 1;
  }
  local_time.tm_hour = (local_time.tm_hour + hour_adjust + carry) % 12; // Adjust hours
  local_time.tm_min = (local_time.tm_min + min_adjust) % 60; // Adjust minutes
  return local_time;
}

void prvUpdateTime(void* pvParameters)
{
  if (clockMutex == NULL) {
    Serial.println("clockMutex is NULL, cannot proceed.");
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
      xSemaphoreTake(clockMutex, portMAX_DELAY);
      Serial.println("Got mutex, updating time...");
      current_time = new_time;
      Serial.printf("Updated time to %d:%d:%d\n", new_time.tm_hour, new_time.tm_min, new_time.tm_sec);
      xSemaphoreGive(clockMutex);
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
        cur_time = current_time;
        xSemaphoreTake(clockMutex, portMAX_DELAY);
        display_data = formatTime(cur_time.tm_hour, cur_time.tm_min);
        xSemaphoreGive(clockMutex);
        Serial.printf("Time updated! New time is %d:%d:%d\n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
        xTaskNotify(display_task, kTimeUpdate, eNoAction);
        colon = !colon; // Toggle colon for display
      }
      if (notificationValue & kAlarmInit) {
        Serial.println("Alarm init");
      }
      if (notificationValue & kAlarmIncMin) {
        Serial.println("Minutes ++");
        if (min_adjust == 59) {
          min_adjust = 0; // Reset to 0 if it was 59
          hour_adjust = (hour_adjust + 1) % 12; // Increment hour and wrap around
        } else {
          min_adjust = (min_adjust + 1) % 60; // Increment min and wrap around
        }
      }
      if (notificationValue & kAlarmIncHr) {
        Serial.println("Hours ++");
        hour_adjust = (hour_adjust + 1) % 12; // Increment hour and wrap around

      }
      if (notificationValue & kAlarmEnd) {
        Serial.println("ALARM!!!!!!!!!!!!!!!");
      }
    }
  }
}

void prvDisplay(void* pvParameters) {
  while (1) {
    uint32_t notificationValue;
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY) == pdTRUE) {
      // Process the notification
      Serial.printf("Display notification received: %d\n", notificationValue);
      xSemaphoreTake(displayMutex, portMAX_DELAY);
      ledMatrix.setTextAlignment(PA_RIGHT);
      ledMatrix.print(display_data.c_str()); // display text
      xSemaphoreGive(displayMutex);
    }
  }
}


void beginTasks() {
  clockMutex = xSemaphoreCreateMutex();
  displayMutex = xSemaphoreCreateMutex();

  xTaskCreate(
    time_tracker::prvDisplay,   // Task function
    "Display",     // Name of the task
    2048,                // Stack size (in words, not bytes)
    NULL,     // Task input parameter
    1,                   // Priority of the task
    &display_task                 // Task handle
  );  

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

std::string formatTime(int hour, int minute) {
  std::ostringstream oss;
  oss.fill('0'); // Set fill character to '0'
  oss.width(2);  // Set width to 2 for hours
  oss << hour;
  if (colon) {
    oss << ":"; // Add colon if needed
  } else {
    oss << " "; // Add space if no colon
  }
  oss.width(2);  // Set width to 2 for minutes
  oss << minute;
  return oss.str();
}

} // namespace time_tracker
