#pragma once

#include <atomic>
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <json/value.h>
#include <memory>
#include <nlohmann/json.hpp>

class Profiler;

class VideoController : public RestController<App> {
public:
  explicit VideoController(App &app,
                           std::shared_ptr<Profiler> profiler = nullptr);
  ~VideoController() override = default;

  void init(std::shared_ptr<Profiler> profiler);

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleGetIndex(const StringHttpRequest &req);
  StringHttpResponse handleServeStatic(const StringHttpRequest &req);
  StringHttpResponse handleListFiles(const StringHttpRequest &req);
  StringHttpResponse handleOpenVideo(const StringHttpRequest &req);
  StringHttpResponse handleOpenAudio(const StringHttpRequest &req);
  StringHttpResponse handleMoveToTrash(const StringHttpRequest &req);
  StringHttpResponse handleGetPlaybackStatus(const StringHttpRequest &req);
  StringHttpResponse handleControlMpv(const StringHttpRequest &req);
  StringHttpResponse handleSeekMpv(const StringHttpRequest &req);
  StringHttpResponse handleFastSeek(const StringHttpRequest &req);
  StringHttpResponse handleGetMpvProperty(const StringHttpRequest &req);
  StringHttpResponse handleCloseVideo(const StringHttpRequest &req);
  StringHttpResponse handleForceStopVideo(const StringHttpRequest &req);
  StringHttpResponse handleDeleteDirectory(const StringHttpRequest &req);
  StringHttpResponse handleSetAudioTrack(const StringHttpRequest &req);
  StringHttpResponse handleExtractThumbnail(const StringHttpRequest &req);

  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;
  int getQueryParamInt(const StringHttpRequest &req, const std::string &key,
                       int defaultValue = 0) const;
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;
  std::string urlDecode(const std::string &str) const;
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
