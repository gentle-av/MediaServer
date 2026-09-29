#pragma once

#include <chrono>

class MonitorService {
public:
  explicit MonitorService(
      std::chrono::milliseconds delay = std::chrono::milliseconds(500));
  ~MonitorService() noexcept = default;

  [[nodiscard]] bool turnOff() noexcept;
  [[nodiscard]] bool turnOn() noexcept;
  [[nodiscard]] bool isOff() const noexcept;

private:
  std::chrono::milliseconds delayDuration;
  bool monitorIsOff;
};
