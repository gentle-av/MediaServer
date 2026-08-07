#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <drogon/drogon.h>
#include <memory>

class MusicMetadataController
    : public drogon::HttpController<MusicMetadataController, false> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicMetadataController::getFileMetadata,
                "/api/music/file-metadata", drogon::Get);
  ADD_METHOD_TO(MusicMetadataController::refreshFileMetadata,
                "/api/music/refresh-metadata", drogon::Post);
  ADD_METHOD_TO(MusicMetadataController::updateFileTags,
                "/api/music/update-tags", drogon::Post);
  ADD_METHOD_TO(MusicMetadataController::getDatabaseStats, "/api/music/stats",
                drogon::Get);
  METHOD_LIST_END

  MusicMetadataController() = default;
  void init(std::shared_ptr<MusicDatabase> db,
            std::shared_ptr<MetadataCache> cache);

  void getFileMetadata(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void refreshFileMetadata(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void updateFileTags(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getDatabaseStats(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<MusicDatabase> db_;
  std::shared_ptr<MetadataCache> cache_;
};
