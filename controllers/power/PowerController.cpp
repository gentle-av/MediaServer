#include "PowerController.h"
#include "profilers/Profiler.h"
#include "services/system/PowerService.h"
#include <nlohmann/json.hpp>

PowerController::PowerController(App &app, std::shared_ptr<Profiler> profiler)
    : RestController<App>(app), profiler(profiler) {
  m_service = std::make_shared<PowerService>();
}

void PowerController::init(std::shared_ptr<Profiler> profiler) {
  this->profiler = profiler;
}

void PowerController::register_all_routes() {
  app_.post("/api/adb/kill-server",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleAdbKillServer(req);
            });
  app_.post("/api/adb/start-server",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleAdbStartServer(req);
            });
  app_.post("/api/adb/connect",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleAdbConnect(req);
            });
  app_.post("/api/adb/keyevent",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleAdbKeyEvent(req);
            });
  app_.get("/api/adb/state",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleAdbGetState(req);
           });
  app_.post("/api/system/sleep",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSystemSleep(req);
            });
  app_.get("/api/power/status",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetPowerStatus(req);
           });
  app_.get("/api/power/tv-state",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetTVPowerState(req);
           });
  app_.post("/api/power/tv-on",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleTvPowerOn(req);
            });
}

nlohmann::json
PowerController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}

std::string
PowerController::getQueryParam(const StringHttpRequest &req,
                               const std::string &key,
                               const std::string &defaultValue) const {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

nlohmann::json
PowerController::jsonValueToNlohmann(const Json::Value &value) const {
  nlohmann::json result;
  if (value.isNull()) {
    return result;
  }
  if (value.isBool()) {
    return value.asBool();
  }
  if (value.isInt()) {
    return value.asInt();
  }
  if (value.isDouble()) {
    return value.asDouble();
  }
  if (value.isString()) {
    return value.asString();
  }
  if (value.isArray()) {
    result = nlohmann::json::array();
    for (const auto &item : value) {
      result.push_back(jsonValueToNlohmann(item));
    }
    return result;
  }
  if (value.isObject()) {
    for (const auto &key : value.getMemberNames()) {
      result[key] = jsonValueToNlohmann(value[key]);
    }
    return result;
  }
  return result;
}

Json::Value PowerController::jsonResponse(bool success,
                                          const std::string &message,
                                          const Json::Value &data) const {
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

StringHttpResponse
PowerController::handleAdbKillServer(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->adbKillServer();
    nlohmann::json response = jsonValueToNlohmann(
        jsonResponse(result["success"].asBool(), result["message"].asString()));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleAdbStartServer(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->adbStartServer();
    nlohmann::json response = jsonValueToNlohmann(
        jsonResponse(result["success"].asBool(), result["message"].asString()));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleAdbConnect(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    std::string address = "192.168.50.13";
    if (!json.is_null() && json.contains("address") &&
        json["address"].is_string()) {
      address = json["address"].get<std::string>();
    }
    Json::Value result = m_service->adbConnect(address);
    Json::Value data;
    data["address"] = result["address"];
    data["output"] = result["output"];
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(
        result["success"].asBool(), result["message"].asString(), data));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleAdbKeyEvent(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("keycode")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Missing keycode parameter";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    int keycode = json["keycode"].get<int>();
    Json::Value result = m_service->adbKeyEvent(keycode);
    Json::Value data;
    data["keycode"] = result["keycode"];
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(
        result["success"].asBool(), result["message"].asString(), data));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleAdbGetState(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->adbGetState();
    Json::Value data;
    data["state"] = result["state"];
    data["connected"] = result["connected"];
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(true, "", data));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleSystemSleep(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->systemSleep();
    nlohmann::json response = jsonValueToNlohmann(
        jsonResponse(result["success"].asBool(), result["message"].asString()));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleGetPowerStatus(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->getPowerStatus();
    Json::Value data;
    data["tv_connected"] = result["tv_connected"];
    data["tv_address"] = result["tv_address"];
    data["media_player_running"] = result["media_player_running"];
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(true, "", data));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleGetTVPowerState(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->getTVPowerState();
    Json::Value data;
    data["tv_address"] = result["tv_address"];
    data["connected"] = result["connected"];
    data["state"] = result["state"];
    data["screen_on"] = result["screen_on"];
    data["wakefulness"] = result["wakefulness"];
    if (result.isMember("raw")) {
      data["raw"] = result["raw"];
    }
    if (result.isMember("error")) {
      data["error"] = result["error"];
    }
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(true, "", data));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PowerController::handleTvPowerOn(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value result = m_service->tvPowerOn();
    nlohmann::json response = jsonValueToNlohmann(jsonResponse(
        result["success"].asBool(), result["message"].asString(), result));
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}
