#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <drogon/drogon.h>

class AlbumManagementController
    : public drogon::HttpController<AlbumManagementController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AlbumManagementController::deleteAlbum,
                "/api/music/delete-album", drogon::Post);
  METHOD_LIST_END

  AlbumManagementController();
  static void init(std::unique_ptr<MusicDatabase> &db,
                   std::unique_ptr<MetadataCache> &cache);

  void
  deleteAlbum(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  static MusicDatabase *db_;
  static MetadataCache *cache_;
};
