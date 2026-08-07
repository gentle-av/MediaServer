#pragma once

#include "database/MusicDatabase.h"
#include "services/music/AlbumArtService.h"
#include "services/music/AlbumArtWriter.h"
#include <drogon/drogon.h>
#include <memory>

class AlbumArtController
    : public drogon::HttpController<AlbumArtController, false> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AlbumArtController::getAlbumArt, "/api/music/albumart",
                drogon::Get);
  ADD_METHOD_TO(AlbumArtController::getAlbumArtByAlbum,
                "/api/music/albumart/album/{album}", drogon::Get);
  ADD_METHOD_TO(AlbumArtController::uploadAlbumArt,
                "/api/music/upload-album-art", drogon::Post);
  ADD_METHOD_TO(AlbumArtController::deleteAlbumArt,
                "/api/music/albumart/delete", drogon::Post);
  ADD_METHOD_TO(AlbumArtController::options, "/api/music/upload-album-art",
                drogon::Options);
  METHOD_LIST_END

  AlbumArtController() = default;
  void init(std::shared_ptr<MusicDatabase> db);

  void
  getAlbumArt(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getAlbumArtByAlbum(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &album);
  void uploadAlbumArt(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void deleteAlbumArt(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void options(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<AlbumArtService> albumArtService_;
  AlbumArtWriter writer_;
};
