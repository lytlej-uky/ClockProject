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
timerArgs timer{0,0};
int min_adjust = 0;
int hour_adjust = 0;
std::string display_data;
bool colon = false;
bool alarmConfig = false;

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
    new_time = getCurrentTime();

    if (new_time.tm_hour != current_time.tm_hour ||
        new_time.tm_min != current_time.tm_min ||
        new_time.tm_sec != current_time.tm_sec) {
      // need to update time!!
      xSemaphoreTake(clockMutex, portMAX_DELAY);
      current_time = new_time;
      xSemaphoreGive(clockMutex);
      xTaskNotify(clock_task, kTimeUpdate, eSetBits);
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
                if (!alarmConfig) {
                    xSemaphoreTake(clockMutex, portMAX_DELAY);
                    display_data = formatTime(cur_time.tm_hour, cur_time.tm_min);
                    xSemaphoreGive(clockMutex);
                    xTaskNotify(display_task, kTimeUpdate, eSetBits);
                }
                colon = !colon; // Toggle colon for display
            }
            if (notificationValue & kAlarmInit) {
                Serial.println("Alarm init triggered");
                xTaskNotify(display_task, kAlarmInit, eSetBits);
                alarmConfig = !alarmConfig; // Toggle alarmConfig
                if (!alarmConfig) {
                  xTaskCreate(
                    time_tracker::prvTimer,   // Task function
                    "Timer",                 // Name of the task
                    2048,                    // Stack size (in words, not bytes)
                    NULL,                  // Task input parameter
                    2,                       // Priority of the task
                    NULL                     // Task handle
                  );
                } else {
                  // Reset display to current time after alarm ends
                  timer.minutes = 0;
                  timer.hours = 0;
                  xSemaphoreTake(clockMutex, portMAX_DELAY);
                  display_data = formatTime(current_time.tm_hour, current_time.tm_min);
                  xSemaphoreGive(clockMutex);
                  xTaskNotify(display_task, kAlarmInit, eSetBits);
                }
            }
            if (notificationValue & kAlarmIncMin) {
                Serial.println("Minutes ++");
                if (alarmConfig) {
                    timer.minutes = (timer.minutes + 1) % 60;
                    display_data = formatTime(timer.hours, timer.minutes);
                    xTaskNotify(display_task, kAlarmInit, eSetBits);
                } else {
                    if (min_adjust == 59) {
                        min_adjust = 0; // Reset to 0 if it was 59
                        hour_adjust = (hour_adjust + 1) % 12; // Increment hour and wrap around
                    } else {
                        min_adjust = (min_adjust + 1) % 60; // Increment min and wrap around
                    }
                }
            }

            if (notificationValue & kAlarmIncHr) {
                Serial.println("Hours ++");
                if (alarmConfig) {
                    timer.hours = (timer.hours + 1) % 12;
                    display_data = formatTime(timer.hours, timer.minutes);
                    xTaskNotify(display_task, kTimeUpdate, eSetBits);
                } else {
                    hour_adjust = (hour_adjust + 1) % 12; // Increment hour and wrap around
                }
            }
            if (notificationValue & kAlarmEnd) {
                Serial.println("ALARM!!!!!!!!!!!!!!!");
                xTaskNotify(display_task, kAlarmEnd, eSetBits);
                alarmConfig = false; // Reset alarmConfig after alarm ends
                // Reset display to current time
                xSemaphoreTake(clockMutex, portMAX_DELAY);
                display_data = formatTime(current_time.tm_hour, current_time.tm_min);
                xSemaphoreGive(clockMutex);
                xTaskNotify(display_task, kTimeUpdate, eSetBits);
            }
        }
    }
}

void prvDisplay(void* pvParameters) {
    while (1) {
        uint32_t notificationValue;
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY) == pdTRUE) {
            // Process the notification
            if (notificationValue & kAlarmInit) {
                Serial.println("Alarm init triggered");
                xSemaphoreTake(displayMutex, portMAX_DELAY);
                ledMatrix.setTextAlignment(PA_CENTER);
                ledMatrix.print(display_data.c_str()); // Display alarm timer
                xSemaphoreGive(displayMutex);
            }
            else if (notificationValue & kAlarmEnd) {
                Serial.println("ALARM!!!!!!!!!!!!!!!");
                xSemaphoreTake(displayMutex, portMAX_DELAY);
                ledMatrix.setTextAlignment(PA_CENTER);
                ledMatrix.print("ALARM"); // Display "ALARM"
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.displayClear();
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.print("ALARM"); // Display "ALARM"
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.displayClear();
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.print("ALARM"); // Display "ALARM"
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.displayClear();
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.print("ALARM"); // Display "ALARM"
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 5s
                ledMatrix.displayClear();
                xSemaphoreGive(displayMutex);
            }
            else {
                Serial.printf("Display notification received: %d\n", notificationValue);
                xSemaphoreTake(displayMutex, portMAX_DELAY);
                ledMatrix.setTextAlignment(PA_RIGHT);
                ledMatrix.print(display_data.c_str()); // Display current time
                xSemaphoreGive(displayMutex);
            }
        }
    }
}

void prvTimer(void* pvParameters) {
  timerArgs timeArg = timer;
  Serial.printf("Timer started with %d hours and %d minutes\n", timeArg.hours, timeArg.minutes);
  timerArgs endTime{current_time.tm_min, current_time.tm_hour};
  endTime.hours = (endTime.hours + timeArg.hours + (endTime.minutes + timeArg.minutes)/60) % 12;
  endTime.minutes = (endTime.minutes + timeArg.minutes) % 60;
  while (1) {
    std::tm cur_time = getCurrentTime();
    if (cur_time.tm_min >= endTime.minutes && cur_time.tm_hour >= endTime.hours) {
      Serial.println("ALARM!!!!!!!!!!!!!!!");
      xTaskNotify(clock_task, kAlarmEnd, eSetBits);
      break; // Exit the loop after alarm is triggered
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1s
  }
  vTaskDelete(NULL); // Delete the task after completion
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
