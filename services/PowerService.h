#pragma once

#include <string>

class PowerService {
public:
  static PowerService &getInstance();

  void adbKillServer();
  void adbStartServer();
  bool adbConnect(const std::string &address = "192.168.50.13");
  bool adbKeyEvent(int keycode);
  std::string adbGetState();
  bool isAdbConnected();

  void systemSleep();
  bool isMediaPlayerRunning();

  struct TVPowerState {
    bool connected;
    bool screenOn;
    std::string wakefulness;
    std::string state;
  };
  TVPowerState getTVPowerState();

private:
  PowerService() = default;
  std::string execCommand(const std::string &cmd, int timeoutSec = 5);
  static constexpr const char *DEFAULT_TV_ADDRESS = "192.168.50.13";
};
