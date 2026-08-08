#pragma once

#include "services/music/MusicScanner.h"
#include <drogon/drogon.h>
#include <memory>

class MusicScanController : public drogon::HttpController<MusicScanController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicScanController::scan, "/api/music/scan", drogon::Post);
  ADD_METHOD_TO(MusicScanController::forceRescan, "/api/music/force-rescan",
                drogon::Post);
  ADD_METHOD_TO(MusicScanController::removeMissing, "/api/music/remove-missing",
                drogon::Post);
  ADD_METHOD_TO(MusicScanController::getRescanStatus,
                "/api/music/rescan-status", drogon::Get);
  METHOD_LIST_END

  MusicScanController() = default;

  void init(std::shared_ptr<MusicDatabase> db,
            std::shared_ptr<MetadataCache> cache,
            std::shared_ptr<MusicScanner> scanner);
  void scan(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  forceRescan(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void removeMissing(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void getRescanStatus(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<MusicScanner> scanner_;
};
