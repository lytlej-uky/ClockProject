#ifndef CLOCK_HPP
#define CLOCK_HPP

#include <freertos/FreeRTOS.h>  // Must be included first
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <chrono>

namespace time_tracker{

const int kTimeUpdate{1};
const int kAlarmInit{2};
const int kAlarmIncMin{4};
const int kAlarmIncHr{8};
const int kAlarmEnd{16};

// Returns the current time as std::tm
std::tm getCurrentTime();

void prvUpdateTime(void* pvParameters);
void prvClockMain(void* pvParameters);
void beginTasks();
void IRAM_ATTR handleInterruptStartTimer();
void IRAM_ATTR handleInterruptIncMins();
void IRAM_ATTR handleInterruptIncHrs();

} // namespace time_tracker
#endif // CLOCK_HPP
