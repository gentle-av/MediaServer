#include "PowerController.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

static std::mutex sleepMutex;
static std::chrono::steady_clock::time_point lastSleepCall;
static bool isGoingToSleep = false;

PowerController::PowerController() {
  lastSleepCall = std::chrono::steady_clock::now();
}

PowerController::~PowerController() {}

Json::Value PowerController::jsonResponse(bool success,
                                          const std::string &message,
                                          const Json::Value &data) {
  Json::Value resp;
  resp["success"] = success;
  if (!message.empty())
    resp["message"] = message;
  if (!data.empty())
    resp["data"] = data;
  return resp;
}

std::string PowerController::execCommand(const std::string &cmd,
                                         int timeoutSec) {
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

bool PowerController::isProcessAlive(const std::string &processName) {
  std::string cmd = "pgrep -f '" + processName + "' 2>/dev/null";
  std::string result = execCommand(cmd, 2);
  return !result.empty();
}

void PowerController::adbKillServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  execCommand("adb kill-server 2>/dev/null", 5);
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "ADB server killed"));
  callback(resp);
}

void PowerController::adbStartServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  execCommand("adb start-server 2>/dev/null", 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "ADB server started"));
  callback(resp);
}

void PowerController::adbConnect(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  std::string address = DEFAULT_TV_ADDRESS;
  if (json && json->isMember("address") && (*json)["address"].isString())
    address = (*json)["address"].asString();
  std::string cmd = "adb connect " + address + " 2>&1";
  std::string result = execCommand(cmd, 5);
  bool success = result.find("connected") != std::string::npos ||
                 result.find("already connected") != std::string::npos;
  Json::Value data;
  data["address"] = address;
  data["output"] = result;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      success, success ? "Connected to TV" : "Connection failed", data));
  callback(resp);
}

void PowerController::adbKeyEvent(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("keycode")) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Missing keycode parameter"));
    callback(resp);
    return;
  }
  int keycode = (*json)["keycode"].asInt();
  std::string cmd =
      "adb shell input keyevent " + std::to_string(keycode) + " 2>&1";
  std::string result = execCommand(cmd, 5);
  bool success = result.empty() || result.find("error") == std::string::npos;
  Json::Value data;
  data["keycode"] = keycode;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      success, success ? "Key event sent" : "Failed to send key event", data));
  callback(resp);
}

void PowerController::adbGetState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string result = execCommand("adb get-state 2>&1", 5);
  bool connected = result.find("device") != std::string::npos;
  Json::Value data;
  data["state"] = result;
  data["connected"] = connected;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::systemSleep(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::lock_guard<std::mutex> lock(sleepMutex);
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - lastSleepCall)
          .count();
  if (isGoingToSleep || elapsed < 10) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Sleep request ignored - too frequent"));
    callback(resp);
    return;
  }
  isGoingToSleep = true;
  lastSleepCall = now;
  std::string cmd = "/usr/bin/systemctl suspend 2>/dev/null";
  int result = system(cmd.c_str());
  bool success = result == 0;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      success, success ? "System going to sleep" : "Failed to sleep"));
  callback(resp);
  isGoingToSleep = false;
}

void PowerController::getPowerStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value data;
  execCommand("adb start-server 2>/dev/null", 2);
  std::string result = execCommand("adb get-state 2>/dev/null", 2);
  bool tvConnected = result.find("device") != std::string::npos;
  data["tv_connected"] = tvConnected;
  data["tv_address"] = DEFAULT_TV_ADDRESS;
  data["media_player_running"] = isProcessAlive("mpv.*--input-ipc-server");
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::getTVPowerState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto self = std::shared_ptr<PowerController>(this, [](PowerController *) {});
  std::thread([self, callback]() {
    Json::Value data;
    data["tv_address"] = DEFAULT_TV_ADDRESS;
    system("timeout 2 adb start-server 2>/dev/null");
    std::string stateResult = "";
    FILE *pipe = popen("timeout 3 adb get-state 2>/dev/null", "r");
    if (pipe) {
      char buffer[128];
      while (fgets(buffer, sizeof(buffer), pipe))
        stateResult += buffer;
      pclose(pipe);
    }
    bool connected = stateResult.find("device") != std::string::npos;
    data["connected"] = connected;
    data["state"] = stateResult.empty() ? "unknown" : stateResult;
    if (!connected) {
      data["screen_on"] = false;
      data["wakefulness"] = "disconnected";
      data["error"] = "ADB not connected to TV";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          self->jsonResponse(true, "", data));
      callback(resp);
      return;
    }
    std::string powerResult = "";
    pipe = popen("timeout 5 adb shell dumpsys power 2>/dev/null | grep -E "
                 "'mWakefulness|Display Power' | head -3",
                 "r");
    if (pipe) {
      char buffer[256];
      while (fgets(buffer, sizeof(buffer), pipe))
        powerResult += buffer;
      pclose(pipe);
    }
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
    } else if (powerResult.find("Display Power: state=ON") !=
               std::string::npos) {
      screenOn = true;
    } else if (powerResult.find("Display Power: state=OFF") !=
               std::string::npos) {
      screenOn = false;
    }
    data["screen_on"] = screenOn;
    data["wakefulness"] = wakefulness;
    data["raw"] = powerResult.empty() ? "No data received" : powerResult;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        self->jsonResponse(true, "", data));
    callback(resp);
  }).detach();
  Json::Value loadingData;
  loadingData["tv_address"] = DEFAULT_TV_ADDRESS;
  loadingData["loading"] = true;
  loadingData["message"] = "Checking TV status...";
  auto loadingResp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Loading", loadingData));
  callback(loadingResp);
}
