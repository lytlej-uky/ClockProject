#ifndef CLOCK_HPP
#define CLOCK_HPP

#include <chrono>
#include <freertos/semphr.h>

namespace time{

const int kTimeUpdate{1};
const int kAlarmStart{3};
const int kAlarmEnd{5};

extern std::tm current_time;
extern SemaphoreHandle_t xMutex;

// Returns the current time as std::tm
std::tm getCurrentTime();

// Task that updates the time
void prvUpdateTime(void* pvParameters);

void prvClockMain(void* pvParameters);

} // namespace time
#endif // CLOCK_HPP
