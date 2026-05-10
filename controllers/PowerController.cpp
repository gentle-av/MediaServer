#include "PowerController.h"
#include "services/system/PowerService.h"

PowerController::PowerController() {
  m_service = std::make_shared<PowerService>();
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

void PowerController::adbKillServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->adbKillServer();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(result["success"].asBool(), result["message"].asString()));
  callback(resp);
}

void PowerController::adbStartServer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->adbStartServer();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(result["success"].asBool(), result["message"].asString()));
  callback(resp);
}

void PowerController::adbConnect(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  std::string address = "192.168.50.13";
  if (json && json->isMember("address") && (*json)["address"].isString())
    address = (*json)["address"].asString();
  Json::Value result = m_service->adbConnect(address);
  Json::Value data;
  data["address"] = result["address"];
  data["output"] = result["output"];
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      result["success"].asBool(), result["message"].asString(), data));
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
  Json::Value result = m_service->adbKeyEvent(keycode);
  Json::Value data;
  data["keycode"] = result["keycode"];
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      result["success"].asBool(), result["message"].asString(), data));
  callback(resp);
}

void PowerController::adbGetState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->adbGetState();
  Json::Value data;
  data["state"] = result["state"];
  data["connected"] = result["connected"];
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::systemSleep(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->systemSleep();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(result["success"].asBool(), result["message"].asString()));
  callback(resp);
}

void PowerController::getPowerStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->getPowerStatus();
  Json::Value data;
  data["tv_connected"] = result["tv_connected"];
  data["tv_address"] = result["tv_address"];
  data["media_player_running"] = result["media_player_running"];
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::getTVPowerState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->getTVPowerState();
  Json::Value data;
  data["tv_address"] = result["tv_address"];
  data["connected"] = result["connected"];
  data["state"] = result["state"];
  data["screen_on"] = result["screen_on"];
  data["wakefulness"] = result["wakefulness"];
  if (result.isMember("raw"))
    data["raw"] = result["raw"];
  if (result.isMember("error"))
    data["error"] = result["error"];
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PowerController::tvPowerOn(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value result = m_service->tvPowerOn();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
      result["success"].asBool(), result["message"].asString(), result));
  callback(resp);
}
