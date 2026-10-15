#pragma once

#include "services/video/SubtitleService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class SubtitleController : public RestController<App> {
public:
  explicit SubtitleController(App &app,
                              std::shared_ptr<SubtitleService> subtitleService);
  ~SubtitleController() = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleGetSubtitleTracks(const StringHttpRequest &req);
  StringHttpResponse handleSetSubtitleTrack(const StringHttpRequest &req);
  StringHttpResponse
  handleGetCurrentSubtitleTrack(const StringHttpRequest &req);
  StringHttpResponse handleDisableSubtitles(const StringHttpRequest &req);

  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;

  std::shared_ptr<SubtitleService> subtitleService;
  static std::string activeSocket;
};
