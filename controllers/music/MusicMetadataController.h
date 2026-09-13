#pragma once

#include "../../database/MusicDatabase.h"
#include "../../services/music/MetadataCache.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class MusicMetadataController : public RestController<App> {
public:
  MusicMetadataController(App &app, std::shared_ptr<MusicDatabase> db,
                          std::shared_ptr<MetadataCache> cache);
  ~MusicMetadataController() = default;

protected:
  void register_all_routes() override;

private:
  std::shared_ptr<MusicDatabase> db;
  std::shared_ptr<MetadataCache> cache;
  StringHttpResponse handleGetFileMetadata(const StringHttpRequest &req);
  StringHttpResponse handleRefreshFileMetadata(const StringHttpRequest &req);
  StringHttpResponse handleUpdateFileTags(const StringHttpRequest &req);
  StringHttpResponse handleGetDatabaseStats(const StringHttpRequest &req);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "");
};
