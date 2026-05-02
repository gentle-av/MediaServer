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
  LOG_INFO << "PowerController initialized";
  lastSleepCall = std::chrono::steady_clock::now();
}

PowerController::~PowerController() { LOG_INFO << "PowerController destroyed"; }

Json::Value PowerController::jsonResponse(bool success,
                                          const std::string &message,
                                          const Json::Value &data) {
  Json::Value resp;
  resp["success"] = success;
  if (!message.empty()) {
    resp["message"] = message;
  }
  if (!data.empty()) {
    resp["data"] = data;
  }
  return resp;
}

std::string PowerController::execCommand(const std::string &cmd) {
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    LOG_ERROR << "Failed to execute command: " << cmd;
    return "";
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  return result;
}

bool PowerController::isProcessAlive(const std::string &processName) {
  std::string cmd = "pgrep -f '" + processName + "' 2>/dev/null";
  std::string result = execCommand(cmd);
  return !result.empty();
}

void PowerController::adbKillServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  LOG_INFO << "adbKillServer called";
  execCommand("adb kill-server 2>/dev/null");
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "ADB server killed"));
  callback(resp);
}

void PowerController::adbStartServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  LOG_INFO << "adbStartServer called";
  execCommand("adb start-server 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "ADB server started"));
  callback(resp);
}

void PowerController::adbConnect(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  LOG_INFO << "adbConnect called";
  auto json = req->getJsonObject();
  std::string address = DEFAULT_TV_ADDRESS;
  if (json && json->isMember("address") && (*json)["address"].isString()) {
    address = (*json)["address"].asString();
  }
  std::string cmd = "adb connect " + address + " 2>&1";
  std::string result = execCommand(cmd);
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
  LOG_INFO << "adbKeyEvent called";
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
  std::string result = execCommand(cmd);
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
  LOG_INFO << "adbGetState called";
  std::string result = execCommand("adb get-state 2>&1");
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
  LOG_INFO << "systemSleep called";
  std::lock_guard<std::mutex> lock(sleepMutex);
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - lastSleepCall)
          .count();
  if (isGoingToSleep || elapsed < 10) {
    LOG_WARN << "Sleep request ignored - already going to sleep or too "
                "frequent (elapsed: "
             << elapsed << "s)";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Sleep request ignored - too frequent"));
    callback(resp);
    return;
  }
  isGoingToSleep = true;
  lastSleepCall = now;
  LOG_INFO << "Executing system sleep...";
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
  LOG_INFO << "getPowerStatus called";
  Json::Value data;
  execCommand("adb start-server 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  execCommand("adb connect " + std::string(DEFAULT_TV_ADDRESS) +
              " 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::string result = execCommand("adb get-state 2>&1");
  bool tvConnected = result.find("device") != std::string::npos;
  data["tv_connected"] = tvConnected;
  data["tv_address"] = DEFAULT_TV_ADDRESS;
  bool mpvAlive = isProcessAlive("mpv.*--input-ipc-server");
  data["media_player_running"] = mpvAlive;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::getTVPowerState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  LOG_INFO << "getTVPowerState called";

  execCommand("adb start-server 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  execCommand("adb connect " + std::string(DEFAULT_TV_ADDRESS) +
              " 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::string result = execCommand(
      "adb shell dumpsys power 2>/dev/null | grep -E "
      "'mHoldingDisplaySuspendBlocker|Display Power|mWakefulness' | head -5");

  bool screenOn = false;
  std::string wakefulness = "Unknown";

  if (result.find("mWakefulness=Awake") != std::string::npos) {
    wakefulness = "Awake";
    screenOn = true;
  } else if (result.find("mWakefulness=Asleep") != std::string::npos) {
    wakefulness = "Asleep";
    screenOn = false;
  } else if (result.find("mHoldingDisplaySuspendBlocker=true") !=
             std::string::npos) {
    screenOn = true;
  }

  Json::Value data;
  data["tv_address"] = DEFAULT_TV_ADDRESS;
  data["connected"] = true;
  data["screen_on"] = screenOn;
  data["wakefulness"] = wakefulness;
  data["raw"] = result;

  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}
