#pragma once
#include "database/MusicDatabase.h"
#include <drogon/drogon.h>

class MusicController : public drogon::HttpController<MusicController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicController::getTracksByArtist,
                "/api/music/tracks/artist/{artist}", drogon::Get);
  ADD_METHOD_TO(MusicController::getTracksByAlbum,
                "/api/music/tracks/album/{album}", drogon::Get);
  ADD_METHOD_TO(MusicController::listFiles, "/api/music/list", drogon::Get);
  ADD_METHOD_TO(MusicController::getArtists, "/api/music/artists", drogon::Get);
  ADD_METHOD_TO(MusicController::getAlbums, "/api/music/albums", drogon::Get);
  ADD_METHOD_TO(MusicController::getAlbumArt, "/api/music/albumart/{path}",
                drogon::Get);
  ADD_METHOD_TO(MusicController::getAlbumArtByAlbum,
                "/api/music/albumart/album/{album}", drogon::Get);
  ADD_METHOD_TO(MusicController::scan, "/api/music/scan", drogon::Post);
  ADD_METHOD_TO(MusicController::removeMissing, "/api/music/remove-missing",
                drogon::Post);
  ADD_METHOD_TO(MusicController::openMusium, "/api/music/open", drogon::Post);
  ADD_METHOD_TO(MusicController::getFileMetadata, "/api/music/file-metadata",
                drogon::Get);
  ADD_METHOD_TO(MusicController::refreshFileMetadata,
                "/api/music/refresh-metadata", drogon::Post);
  ADD_METHOD_TO(MusicController::getDatabaseStats, "/api/music/stats",
                drogon::Get);
  ADD_METHOD_TO(MusicController::forceRescan, "/api/music/force-rescan",
                drogon::Post);
  METHOD_LIST_END

  MusicController();

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
  void
  getAlbumArt(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback,
              const std::string &path);
  void getAlbumArtByAlbum(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &album);
  void scan(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void removeMissing(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  openMusium(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getFileMetadata(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void refreshFileMetadata(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getDatabaseStats(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  forceRescan(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::unique_ptr<MusicDatabase> db_;
  std::string musicDir_;
  bool extractMetadata(const std::string &filePath, MusicMetadata &metadata);
  bool extractAlbumArt(const std::string &filePath,
                       std::vector<char> &albumArt);
  void scanNewFiles();
  void removeMissingFiles();
};
