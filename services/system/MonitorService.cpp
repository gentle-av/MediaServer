#include "MonitorService.h"
#include <cstdlib>
#include <sys/wait.h>
#include <thread>

MonitorService::MonitorService(std::chrono::milliseconds delay)
    : delayDuration(delay), monitorIsOff(false) {}

bool MonitorService::turnOff() noexcept {
  if (monitorIsOff) {
    return true;
  }
  if (delayDuration.count() > 0) {
    std::this_thread::sleep_for(delayDuration);
  }
  int status = std::system("ddcutil setvcp D6 4 > /dev/null 2>&1");
  if (status == -1) {
    return false;
  }
  int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  if (exitCode == 0 || exitCode == 1) {
    monitorIsOff = true;
    return true;
  }
  return false;
}

bool MonitorService::turnOn() noexcept {
  if (!monitorIsOff) {
    return true;
  }
  int status = std::system("ddcutil setvcp D6 1 > /dev/null 2>&1");
  if (status == -1) {
    return false;
  }
  int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  if (exitCode == 0 || exitCode == 1) {
    monitorIsOff = false;
    return true;
  }
  return false;
}

bool MonitorService::isOff() const noexcept { return monitorIsOff; }
