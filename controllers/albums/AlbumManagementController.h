#pragma once

#include "../../database/MusicDatabase.h"
#include "../../repositories/MusicRepository.h"
#include "../../services/music/MetadataCache.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class AlbumManagementController : public RestController<App> {
public:
  explicit AlbumManagementController(App &app,
                                     std::shared_ptr<MusicDatabase> db,
                                     std::shared_ptr<MetadataCache> cache,
                                     MusicRepository &repo);
  ~AlbumManagementController() = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleDeleteAlbum(const StringHttpRequest &req);
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;
  void logAlbumState(const std::string &tag, const std::string &album,
                     const std::string &artist);

  std::shared_ptr<MusicDatabase> db;
  std::shared_ptr<MetadataCache> cache;
  MusicRepository &musicRepository;
};
