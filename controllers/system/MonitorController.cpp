#include "controllers/system/MonitorController.h"
#include "profilers/Profiler.h"

extern Profiler *g_profiler;

void MonitorController::ensureService() {
  if (!m_service) {
    m_service = std::make_unique<MonitorService>();
  }
  if (!profiler_) {
    profiler_ = g_profiler;
  }
}

void MonitorController::isSessionIdle(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &resp)> &&callback) {
  ensureService();

  Json::Value response;
  try {
    bool isIdle = m_service->isSessionIdle();
    response["success"] = true;
    response["isIdle"] = isIdle;
    response["idleTimeoutMs"] = 60000;
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
    return;
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void MonitorController::turnOnMonitor(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &resp)> &&callback) {
  ensureService();
  Json::Value response;
  try {
    m_service->turnOnDisplay();
    response["success"] = true;
    response["message"] = "Monitor turned on successfully";
    response["action"] = "turn_on";
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
    return;
  }

  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void MonitorController::turnOffMonitor(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &resp)> &&callback) {
  ensureService();

  Json::Value response;
  try {
    m_service->turnOffDisplay();
    response["success"] = true;
    response["message"] = "Monitor turned off successfully";
    response["action"] = "turn_off";
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
    return;
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}
