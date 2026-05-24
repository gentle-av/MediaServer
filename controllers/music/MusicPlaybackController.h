#pragma once

#include "controllers/player/PlayerController.h"
#include "services/music/PlaylistManager.h"
#include <drogon/drogon.h>

class MusicPlaybackController
    : public drogon::HttpController<MusicPlaybackController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicPlaybackController::openMusium, "/api/music/open",
                drogon::Post);
  ADD_METHOD_TO(MusicPlaybackController::openAlbum,
                "/api/music/open/album/{album}", drogon::Post);
  ADD_METHOD_TO(MusicPlaybackController::openArtist,
                "/api/music/open/artist/{artist}", drogon::Post);
  METHOD_LIST_END

  MusicPlaybackController();
  static void init(std::unique_ptr<MusicDatabase> &db,
                   std::shared_ptr<PlayerController> playerController);

  void
  openMusium(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  openAlbum(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::string &album);
  void
  openArtist(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             const std::string &artist);

private:
  static std::unique_ptr<PlaylistManager> playlistManager_;
};
