#pragma once

class MonitorService {
public:
  static void turnOnDisplay();
  static void turnOffDisplay();
  static bool isSessionIdle();
};
