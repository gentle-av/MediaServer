#pragma once

class MonitorService {
public:
  void turnOnDisplay();
  void turnOffDisplay();
  bool isSessionIdle();
  bool isInitialized() const { return m_initialized; }

private:
  bool m_initialized = false;
  bool init();
};
