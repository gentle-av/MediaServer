#include "services/system/PowerService.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

PowerService::PowerService() {
  m_lastSleepCall = std::chrono::steady_clock::now();
  m_isGoingToSleep = false;
}

PowerService::~PowerService() {}

std::string PowerService::execCommand(const std::string &cmd, int timeoutSec) {
  std::string cmdWithTimeout =
      "timeout " + std::to_string(timeoutSec) + " " + cmd;
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmdWithTimeout.c_str(), "r");
  if (!pipe)
    return "";
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    result += buffer.data();
  pclose(pipe);
  return result;
}

bool PowerService::isProcessAlive(const std::string &processName) {
  std::string cmd = "pgrep -f '" + processName + "' 2>/dev/null";
  std::string result = execCommand(cmd, 2);
  return !result.empty();
}

bool PowerService::ensureAdbConnected(const std::string &address,
                                      int maxAttempts) {
  execCommand("adb start-server 2>/dev/null", 2);
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    std::string result = execCommand("adb get-state 2>/dev/null", 2);
    if (result.find("device") != std::string::npos)
      return true;
    std::string connectCmd = "adb connect " + address + " 2>&1";
    std::string connectResult = execCommand(connectCmd, 3);
    if (connectResult.find("connected") != std::string::npos ||
        connectResult.find("already connected") != std::string::npos) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      result = execCommand("adb get-state 2>/dev/null", 2);
      if (result.find("device") != std::string::npos)
        return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  return false;
}

bool PowerService::getTVScreenState() {
  std::string powerResult =
      execCommand("adb shell dumpsys power 2>/dev/null | grep -E "
                  "'mWakefulness|Display Power' | head -1",
                  5);
  return powerResult.find("mWakefulness=Awake") != std::string::npos ||
         powerResult.find("Display Power: state=ON") != std::string::npos;
}

Json::Value PowerService::adbKillServer() {
  Json::Value result;
  execCommand("adb kill-server 2>/dev/null", 5);
  result["success"] = true;
  result["message"] = "ADB server killed";
  return result;
}

Json::Value PowerService::adbStartServer() {
  Json::Value result;
  execCommand("adb start-server 2>/dev/null", 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  result["success"] = true;
  result["message"] = "ADB server started";
  return result;
}

Json::Value PowerService::adbConnect(const std::string &address) {
  Json::Value result;
  std::string cmd = "adb connect " + address + " 2>&1";
  std::string output = execCommand(cmd, 5);
  bool success = output.find("connected") != std::string::npos ||
                 output.find("already connected") != std::string::npos;
  result["success"] = success;
  result["message"] = success ? "Connected to TV" : "Connection failed";
  result["address"] = address;
  result["output"] = output;
  return result;
}

Json::Value PowerService::adbKeyEvent(int keycode) {
  Json::Value result;
  std::string cmd =
      "adb shell input keyevent " + std::to_string(keycode) + " 2>&1";
  std::string output = execCommand(cmd, 5);
  bool success = output.empty() || output.find("error") == std::string::npos;
  result["success"] = success;
  result["message"] = success ? "Key event sent" : "Failed to send key event";
  result["keycode"] = keycode;
  return result;
}

Json::Value PowerService::adbGetState() {
  Json::Value result;
  std::string output = execCommand("adb get-state 2>&1", 5);
  bool connected = output.find("device") != std::string::npos;
  result["success"] = true;
  result["state"] = output;
  result["connected"] = connected;
  return result;
}

Json::Value PowerService::systemSleep() {
  Json::Value result;
  std::lock_guard<std::mutex> lock(m_sleepMutex);
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - m_lastSleepCall)
          .count();
  if (m_isGoingToSleep || elapsed < 10) {
    result["success"] = false;
    result["message"] = "Sleep request ignored - too frequent";
    return result;
  }
  m_isGoingToSleep = true;
  m_lastSleepCall = now;
  std::string cmd = "/usr/bin/systemctl suspend 2>/dev/null";
  int ret = system(cmd.c_str());
  bool success = ret == 0;
  result["success"] = success;
  result["message"] = success ? "System going to sleep" : "Failed to sleep";
  m_isGoingToSleep = false;
  return result;
}

Json::Value PowerService::getPowerStatus() {
  Json::Value result;
  execCommand("adb start-server 2>/dev/null", 2);
  std::string stateResult = execCommand("adb get-state 2>/dev/null", 2);
  bool tvConnected = stateResult.find("device") != std::string::npos;
  result["success"] = true;
  result["tv_connected"] = tvConnected;
  result["tv_address"] = DEFAULT_TV_ADDRESS;
  result["media_player_running"] = isProcessAlive("mpv.*--input-ipc-server");
  return result;
}

Json::Value PowerService::getTVPowerState() {
  Json::Value result;
  result["tv_address"] = DEFAULT_TV_ADDRESS;
  execCommand("adb start-server 2>/dev/null", 2);
  std::string stateResult = execCommand("adb get-state 2>/dev/null", 3);
  bool connected = stateResult.find("device") != std::string::npos;
  result["connected"] = connected;
  result["state"] = stateResult.empty() ? "unknown" : stateResult;
  if (!connected) {
    result["screen_on"] = false;
    result["wakefulness"] = "disconnected";
    result["error"] = "ADB not connected to TV";
    return result;
  }
  std::string powerResult =
      execCommand("adb shell dumpsys power 2>/dev/null | grep -E "
                  "'mWakefulness|Display Power' | head -3",
                  5);
  bool screenOn = false;
  std::string wakefulness = "Unknown";
  if (powerResult.find("mWakefulness=Awake") != std::string::npos) {
    wakefulness = "Awake";
    screenOn = true;
  } else if (powerResult.find("mWakefulness=Asleep") != std::string::npos) {
    wakefulness = "Asleep";
    screenOn = false;
  } else if (powerResult.find("mWakefulness=Dozing") != std::string::npos) {
    wakefulness = "Dozing";
    screenOn = false;
  } else if (powerResult.find("Display Power: state=ON") != std::string::npos) {
    screenOn = true;
  } else if (powerResult.find("Display Power: state=OFF") !=
             std::string::npos) {
    screenOn = false;
  }
  result["screen_on"] = screenOn;
  result["wakefulness"] = wakefulness;
  result["raw"] = powerResult.empty() ? "No data received" : powerResult;
  return result;
}

Json::Value PowerService::tvPowerOn() {
  Json::Value result;
  if (!ensureAdbConnected(DEFAULT_TV_ADDRESS, 3)) {
    result["success"] = false;
    result["error"] = "Failed to connect to TV via ADB";
    return result;
  }
  Json::Value keyResult = adbKeyEvent(26);
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  bool screenOn = getTVScreenState();
  result["success"] = screenOn;
  result["screen_on"] = screenOn;
  result["keycode_sent"] = keyResult["success"].asBool();
  result["message"] = screenOn ? "TV powered on successfully"
                               : "Key sent but TV did not turn on";
  return result;
}
