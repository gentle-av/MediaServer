#include "MonitorController.h"
#include "profilers/Profiler.h"
#include "services/system/MonitorService.h"

std::string MonitorController::activeSocket = "";

MonitorController::MonitorController(App &app,
                                     std::shared_ptr<Profiler> profiler)
    : RestController<App>(app), profiler(profiler) {}

void MonitorController::init(std::shared_ptr<Profiler> profiler) {
  this->profiler = profiler;
}

void MonitorController::register_all_routes() {
  app_.get("/api/monitor/is_idle",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleIsSessionIdle(req);
           });
  app_.get("/api/monitor/status",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetMonitorStatus(req);
           });
  app_.post("/api/monitor/turn_on",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleTurnOnMonitor(req);
            });
  app_.post("/api/monitor/turn_off",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleTurnOffMonitor(req);
            });
}

nlohmann::json
MonitorController::jsonValueToNlohmann(const Json::Value &value) const {
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

StringHttpResponse
MonitorController::handleIsSessionIdle(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    MonitorService service;
    bool isIdle = service.isSessionIdle();
    nlohmann::json response;
    response["success"] = true;
    response["isIdle"] = isIdle;
    response["idleTimeoutMs"] = 60000;
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
MonitorController::handleGetMonitorStatus(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (statusRequestInProgress.exchange(true)) {
      std::lock_guard<std::mutex> lock(statusMutex);
      if (statusCache.isValid) {
        res.setJsonContent(statusCache.data.dump());
        statusRequestInProgress = false;
        res.setStatus(200);
        return res;
      }
      statusRequestInProgress = false;
    }
    {
      std::lock_guard<std::mutex> lock(statusMutex);
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - statusCache.timestamp)
                         .count();
      if (statusCache.isValid && elapsed < 200) {
        res.setJsonContent(statusCache.data.dump());
        statusRequestInProgress = false;
        res.setStatus(200);
        return res;
      }
    }
    MonitorService service;
    bool isIdle = service.isSessionIdle();
    nlohmann::json response;
    response["success"] = true;
    response["data"]["is_idle"] = isIdle;
    response["data"]["idleTimeoutMs"] = 60000;
    {
      std::lock_guard<std::mutex> lock(statusMutex);
      statusCache.data = response;
      statusCache.timestamp = std::chrono::steady_clock::now();
      statusCache.isValid = true;
    }
    res.setJsonContent(response.dump());
    statusRequestInProgress = false;
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
    statusRequestInProgress = false;
  }
  return res;
}

StringHttpResponse
MonitorController::handleTurnOnMonitor(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    MonitorService service;
    service.turnOnDisplay();
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Monitor turned on successfully";
    response["action"] = "turn_on";
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
MonitorController::handleTurnOffMonitor(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    MonitorService service;
    service.turnOffDisplay();
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Monitor turned off successfully";
    response["action"] = "turn_off";
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
