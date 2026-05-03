#include "services/PowerService.h"
#include <array>
#include <chrono>
#include <thread>

PowerService &PowerService::getInstance() {
  static PowerService instance;
  return instance;
}

std::string PowerService::execCommand(const std::string &cmd, int timeoutSec) {
  std::string cmdWithTimeout =
      "timeout " + std::to_string(timeoutSec) + " " + cmd;
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmdWithTimeout.c_str(), "r");
  if (!pipe)
    return "";
  while (fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  pclose(pipe);
  return result;
}

void PowerService::adbKillServer() {
  execCommand("adb kill-server 2>/dev/null", 5);
}

void PowerService::adbStartServer() {
  execCommand("adb start-server 2>/dev/null", 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

bool PowerService::adbConnect(const std::string &address) {
  std::string result = execCommand("adb connect " + address + " 2>&1", 5);
  return result.find("connected") != std::string::npos ||
         result.find("already connected") != std::string::npos;
}

bool PowerService::adbKeyEvent(int keycode) {
  std::string result = execCommand(
      "adb shell input keyevent " + std::to_string(keycode) + " 2>&1", 5);
  return result.empty() || result.find("error") == std::string::npos;
}

std::string PowerService::adbGetState() {
  return execCommand("adb get-state 2>&1", 5);
}

bool PowerService::isAdbConnected() {
  return adbGetState().find("device") != std::string::npos;
}

void PowerService::systemSleep() {
  execCommand("/usr/bin/systemctl suspend 2>/dev/null", 30);
}

bool PowerService::isMediaPlayerRunning() {
  std::string result =
      execCommand("pgrep -f 'mpv.*--input-ipc-server' 2>/dev/null", 2);
  return !result.empty();
}

PowerService::TVPowerState PowerService::getTVPowerState() {
  TVPowerState state;
  state.connected = false;
  state.screenOn = false;
  state.wakefulness = "Unknown";
  adbStartServer();
  state.connected = isAdbConnected();
  state.state = adbGetState();
  if (!state.connected)
    return state;
  std::string powerResult =
      execCommand("adb shell dumpsys power 2>/dev/null | grep -E "
                  "'mWakefulness|Display Power' | head -3",
                  5);
  if (powerResult.find("mWakefulness=Awake") != std::string::npos) {
    state.wakefulness = "Awake";
    state.screenOn = true;
  } else if (powerResult.find("mWakefulness=Asleep") != std::string::npos) {
    state.wakefulness = "Asleep";
    state.screenOn = false;
  } else if (powerResult.find("mWakefulness=Dozing") != std::string::npos) {
    state.wakefulness = "Dozing";
    state.screenOn = false;
  } else if (powerResult.find("Display Power: state=ON") != std::string::npos) {
    state.screenOn = true;
  } else if (powerResult.find("Display Power: state=OFF") !=
             std::string::npos) {
    state.screenOn = false;
  }
  return state;
}
