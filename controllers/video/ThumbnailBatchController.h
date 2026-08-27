#pragma once

#include "../../services/video/ThumbnailBatchScanner.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <nlohmann/json.hpp>

class ThumbnailBatchController : public RestController<App> {
public:
  ThumbnailBatchController(App &app, ImageDatabase &database);
  ~ThumbnailBatchController() override = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleStartScan(const StringHttpRequest &req);
  StringHttpResponse handleStopScan(const StringHttpRequest &req);
  StringHttpResponse handleGetProgress(const StringHttpRequest &req);
  StringHttpResponse handleGetStatus(const StringHttpRequest &req);

  ThumbnailBatchScanner scanner;
  nlohmann::json
  progressToJson(const ThumbnailBatchScanner::Progress &progress) const;
};
