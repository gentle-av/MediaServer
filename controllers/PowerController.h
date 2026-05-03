#pragma once
#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <string>

class PowerController : public drogon::HttpController<PowerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PowerController::adbKillServer, "/api/adb/kill-server",
                drogon::Post);
  ADD_METHOD_TO(PowerController::adbStartServer, "/api/adb/start-server",
                drogon::Post);
  ADD_METHOD_TO(PowerController::adbConnect, "/api/adb/connect", drogon::Post);
  ADD_METHOD_TO(PowerController::adbKeyEvent, "/api/adb/keyevent",
                drogon::Post);
  ADD_METHOD_TO(PowerController::adbGetState, "/api/adb/state", drogon::Get);
  ADD_METHOD_TO(PowerController::systemSleep, "/api/system/sleep",
                drogon::Post);
  ADD_METHOD_TO(PowerController::getPowerStatus, "/api/power/status",
                drogon::Get);
  ADD_METHOD_TO(PowerController::getTVPowerState, "/api/power/tv-state",
                drogon::Get);
  METHOD_LIST_END

  PowerController();
  ~PowerController();
  void adbKillServer(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void adbStartServer(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  adbConnect(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  adbKeyEvent(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  adbGetState(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  systemSleep(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getPowerStatus(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getTVPowerState(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
  std::string execCommand(const std::string &cmd, int timeoutSec = 5);
  bool isProcessAlive(const std::string &processName);
  static constexpr const char *DEFAULT_TV_ADDRESS = "192.168.50.13";
};
