#include "ThumbnailBatchController.h"

ThumbnailBatchController::ThumbnailBatchController(App &app,
                                                   ImageDatabase &database)
    : RestController<App>(app), scanner(database) {}

void ThumbnailBatchController::register_all_routes() {
  app_.post("/api/thumbnails/scan",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleStartScan(req);
            });
  app_.post("/api/thumbnails/scan/stop",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleStopScan(req);
            });
  app_.get("/api/thumbnails/scan/progress",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetProgress(req);
           });
  app_.get("/api/thumbnails/scan/status",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetStatus(req);
           });
}

StringHttpResponse
ThumbnailBatchController::handleStartScan(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json response;
  try {
    auto body = nlohmann::json::parse(req.getBodyString());
    std::string directory = body.value("directory", "/mnt/video");
    bool forceRegenerate = body.value("forceRegenerate", false);
    if (scanner.isRunning()) {
      response["success"] = false;
      response["error"] = "Scan already in progress";
      res.setJsonContent(response.dump());
      res.setStatus(409);
      return res;
    }
    auto callback = [](const ThumbnailBatchScanner::Progress &) {};
    scanner.startScan(directory, forceRegenerate, callback);
    response["success"] = true;
    response["message"] = "Thumbnail scan started";
    response["directory"] = directory;
    response["forceRegenerate"] = forceRegenerate;
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
ThumbnailBatchController::handleStopScan(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json response;
  scanner.stopScan();
  response["success"] = true;
  response["message"] = "Scan stopped";
  res.setJsonContent(response.dump());
  res.setStatus(200);
  return res;
}

StringHttpResponse
ThumbnailBatchController::handleGetProgress(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json response;
  auto progress = scanner.getProgress();
  response["success"] = true;
  response["progress"] = progressToJson(progress);
  res.setJsonContent(response.dump());
  res.setStatus(200);
  return res;
}

StringHttpResponse
ThumbnailBatchController::handleGetStatus(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json response;
  auto progress = scanner.getProgress();
  response["success"] = true;
  response["isRunning"] = scanner.isRunning();
  response["totalFiles"] = progress.totalFiles;
  response["processedFiles"] = progress.processedFiles;
  response["successfulThumbnails"] = progress.successfulThumbnails;
  response["failedFiles"] = progress.failedFiles;
  response["currentFile"] = progress.currentFile;
  if (progress.totalFiles > 0) {
    response["progressPercent"] =
        (progress.processedFiles * 100.0) / progress.totalFiles;
  } else {
    response["progressPercent"] = 0;
  }
  res.setJsonContent(response.dump());
  res.setStatus(200);
  return res;
}

nlohmann::json ThumbnailBatchController::progressToJson(
    const ThumbnailBatchScanner::Progress &progress) const {
  nlohmann::json j;
  j["totalFiles"] = progress.totalFiles;
  j["processedFiles"] = progress.processedFiles;
  j["successfulThumbnails"] = progress.successfulThumbnails;
  j["failedFiles"] = progress.failedFiles;
  j["isRunning"] = progress.isRunning;
  j["currentFile"] = progress.currentFile;
  if (progress.totalFiles > 0) {
    j["progressPercent"] =
        (progress.processedFiles * 100.0) / progress.totalFiles;
  } else {
    j["progressPercent"] = 0;
  }
  return j;
}
