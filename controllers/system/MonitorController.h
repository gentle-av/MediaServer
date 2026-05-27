#pragma once

#include "services/system/MonitorService.h"
#include <drogon/HttpController.h>

class Profiler;

class MonitorController : public drogon::HttpController<MonitorController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MonitorController::isSessionIdle, "/api/monitor/is_idle",
                drogon::Get);
  ADD_METHOD_TO(MonitorController::getMonitorStatus, "/api/monitor/status",
                drogon::Get);
  ADD_METHOD_TO(MonitorController::turnOnMonitor, "/api/monitor/turn_on",
                drogon::Post);
  ADD_METHOD_TO(MonitorController::turnOffMonitor, "/api/monitor/turn_off",
                drogon::Post);
  METHOD_LIST_END

  void setProfiler(Profiler *profiler) { profiler_ = profiler; }

  void isSessionIdle(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &resp)> &&callback);
  void getMonitorStatus(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &resp)> &&callback);
  void turnOnMonitor(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &resp)> &&callback);
  void turnOffMonitor(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &resp)> &&callback);

private:
  Profiler *profiler_ = nullptr;
  std::unique_ptr<MonitorService> m_service = nullptr;

  void ensureService();
};
