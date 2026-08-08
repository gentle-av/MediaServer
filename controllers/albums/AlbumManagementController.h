#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <drogon/drogon.h>
#include <memory>

class AlbumManagementController
    : public drogon::HttpController<AlbumManagementController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AlbumManagementController::deleteAlbum,
                "/api/music/delete-album", drogon::Post);
  METHOD_LIST_END

  AlbumManagementController() = default;
  void init(std::shared_ptr<MusicDatabase> db,
            std::shared_ptr<MetadataCache> cache);

  void
  deleteAlbum(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<MusicDatabase> db_;
  std::shared_ptr<MetadataCache> cache_;
};
