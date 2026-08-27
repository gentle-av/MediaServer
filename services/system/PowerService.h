#pragma once

#include <json/json.h>
#include <mutex>
#include <string>

class PowerService {
public:
  PowerService();
  ~PowerService();
  Json::Value adbKillServer();
  Json::Value adbStartServer();
  Json::Value adbConnect(const std::string &address);
  Json::Value adbKeyEvent(int keycode);
  Json::Value adbGetState();
  Json::Value systemSleep();
  Json::Value getPowerStatus();
  Json::Value getTVPowerState();
  Json::Value tvPowerOn();
  bool isInitialized() const { return initialized; }

private:
  std::string execCommand(const std::vector<std::string> &args,
                          int timeoutSec = 5);
  bool isProcessAlive(const std::string &processName);
  bool ensureAdbConnected(const std::string &address, int maxAttempts = 3);
  bool getTVScreenState();
  static constexpr const char *DEFAULT_TV_ADDRESS = "192.168.50.13";
  std::mutex sleepMutex;
  std::chrono::steady_clock::time_point lastSleepCall;
  bool isGoingToSleep;
  bool initialized;
};
