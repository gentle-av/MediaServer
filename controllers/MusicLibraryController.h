#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <drogon/drogon.h>

class MusicLibraryController
    : public drogon::HttpController<MusicLibraryController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicLibraryController::getTracksByArtist,
                "/api/music/tracks/artist/{artist}", drogon::Get);
  ADD_METHOD_TO(MusicLibraryController::getTracksByAlbum,
                "/api/music/tracks/album/{album}", drogon::Get);
  ADD_METHOD_TO(MusicLibraryController::listFiles, "/api/music/list",
                drogon::Get);
  ADD_METHOD_TO(MusicLibraryController::getArtists, "/api/music/artists",
                drogon::Get);
  ADD_METHOD_TO(MusicLibraryController::getAlbums, "/api/music/albums",
                drogon::Get);
  ADD_METHOD_TO(MusicLibraryController::getAlbumsPaginated,
                "/api/music/albums/paginated", drogon::Get);
  METHOD_LIST_END

  MusicLibraryController();
  static void init(std::unique_ptr<MusicDatabase> &db,
                   std::unique_ptr<MetadataCache> &cache);

  void getTracksByArtist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &artist);
  void getTracksByAlbum(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &album);
  void
  listFiles(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  getArtists(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  getAlbums(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getAlbumsPaginated(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  static MusicDatabase *db_;
  static MetadataCache *cache_;
};
