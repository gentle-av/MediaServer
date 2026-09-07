#pragma once

#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <json/value.h>
#include <memory>
#include <string>

class PowerService;
class Profiler;

class PowerController : public RestController<App> {
public:
  explicit PowerController(App &app,
                           std::shared_ptr<Profiler> profiler = nullptr);
  ~PowerController() override = default;

  void init(std::shared_ptr<Profiler> profiler);

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleAdbKillServer(const StringHttpRequest &req);
  StringHttpResponse handleAdbStartServer(const StringHttpRequest &req);
  StringHttpResponse handleAdbConnect(const StringHttpRequest &req);
  StringHttpResponse handleAdbKeyEvent(const StringHttpRequest &req);
  StringHttpResponse handleAdbGetState(const StringHttpRequest &req);
  StringHttpResponse handleSystemSleep(const StringHttpRequest &req);
  StringHttpResponse handleGetPowerStatus(const StringHttpRequest &req);
  StringHttpResponse handleGetTVPowerState(const StringHttpRequest &req);
  StringHttpResponse handleTvPowerOn(const StringHttpRequest &req);

  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;
  nlohmann::json jsonValueToNlohmann(const Json::Value &value) const;
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value()) const;

  std::shared_ptr<PowerService> m_service;
  std::shared_ptr<Profiler> profiler;
};
