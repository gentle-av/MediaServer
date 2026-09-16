#pragma once

#include <atomic>
#include <chrono>
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <json/value.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

class Profiler;

class MonitorController : public RestController<App> {
public:
  explicit MonitorController(App &app,
                             std::shared_ptr<Profiler> profiler = nullptr);
  ~MonitorController() override = default;

  void init(std::shared_ptr<Profiler> profiler);

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleIsSessionIdle(const StringHttpRequest &req);
  StringHttpResponse handleGetMonitorStatus(const StringHttpRequest &req);
  StringHttpResponse handleTurnOnMonitor(const StringHttpRequest &req);
  StringHttpResponse handleTurnOffMonitor(const StringHttpRequest &req);
  nlohmann::json jsonValueToNlohmann(const Json::Value &value) const;

  std::shared_ptr<Profiler> profiler;
  static std::string activeSocket;

  struct CachedStatus {
    nlohmann::json data;
    std::chrono::steady_clock::time_point timestamp;
    bool isValid = false;
  };

  CachedStatus statusCache;
  std::mutex statusMutex;
  std::atomic<bool> statusRequestInProgress{false};
};
