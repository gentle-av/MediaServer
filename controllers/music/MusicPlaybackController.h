#pragma once

#include "database/MusicDatabase.h"
#include "services/player/AudioPlaybackService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class MusicPlaybackController : public RestController<App> {
public:
  MusicPlaybackController(
      App &app, std::shared_ptr<MusicDatabase> db,
      std::shared_ptr<AudioPlaybackService> playbackService);
  ~MusicPlaybackController() = default;

protected:
  void register_all_routes() override;

private:
  std::shared_ptr<MusicDatabase> db;
  std::shared_ptr<AudioPlaybackService> playbackService;
  StringHttpResponse handleOpenMusium(const StringHttpRequest &req);
  StringHttpResponse handleOpenAlbum(const StringHttpRequest &req);
  StringHttpResponse handleOpenArtist(const StringHttpRequest &req);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "");
};
