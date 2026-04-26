#pragma once

#include "database/MusicDatabase.h"
#include "services/PlayerService.h"
#include <chrono>
#include <drogon/drogon.h>
#include <mutex>
#include <taglib/tstring.h>
#include <unordered_map>

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
  ADD_METHOD_TO(MusicController::getAlbumsPaginated,
                "/api/music/albums/paginated", drogon::Get);
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
  ADD_METHOD_TO(MusicController::updateFileTags, "/api/music/update-tags",
                drogon::Post);
  ADD_METHOD_TO(MusicController::deleteAlbum, "/api/music/delete-album",
                drogon::Post);
  ADD_METHOD_TO(MusicController::getRescanStatus, "/api/music/rescan-status",
                drogon::Get);
  METHOD_LIST_END

  MusicController();
  static void setPlayerService(std::shared_ptr<PlayerService> service);

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
  void updateFileTags(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  deleteAlbum(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getRescanStatus(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::unique_ptr<MusicDatabase> db_;
  std::string musicDir_;
  static std::shared_ptr<PlayerService> playerService_;
  struct RescanStatus {
    bool inProgress = false;
    int totalFiles = 0;
    int processedFiles = 0;
    int addedFiles = 0;
    int errorCount = 0;
    int oldAlbumsCount = 0;
    int newAlbumsCount = 0;
    std::chrono::steady_clock::time_point lastRescanTime;
  };
  struct CachedMetadata {
    MusicMetadata metadata;
    std::chrono::steady_clock::time_point lastAccess;
    std::vector<char> albumArt;
    bool hasAlbumArt = false;
  };
  std::string fixTagLibString(const TagLib::String &str);
  static RescanStatus rescanStatus_;
  static std::mutex rescanStatusMutex_;
  std::unordered_map<std::string, CachedMetadata> metadataCache_;
  std::mutex cacheMutex_;
  static constexpr size_t MAX_CACHE_SIZE = 500;
  static constexpr int DEFAULT_PAGE_SIZE = 20;
  static constexpr int MAX_PAGE_SIZE = 50;
  bool extractMetadata(const std::string &filePath, MusicMetadata &metadata);
  bool extractMetadataWithTagEditor(const std::string &filePath,
                                    MusicMetadata &metadata);
  bool updateFileTagsInternal(const std::string &filePath,
                              const MusicMetadata &metadata);
  bool extractAlbumArt(const std::string &filePath,
                       std::vector<char> &albumArt);
  void scanNewFiles();
  void removeMissingFiles();
  MusicMetadata *getMetadataFromCache(const std::string &filePath);
  void addMetadataToCache(const std::string &filePath,
                          const MusicMetadata &metadata);
  void cleanupCache();
};
