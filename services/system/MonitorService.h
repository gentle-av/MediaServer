#pragma once

class MonitorService {
public:
  void turnOnDisplay();
  void turnOffDisplay();
  bool isSessionIdle();
  bool isInitialized() const { return initialized; }

private:
  bool initialized = false;
  bool init();
};
