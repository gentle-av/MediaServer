// MusicScanController.cpp
#include "controllers/music/MusicScanController.h"
#include "services/music/ResponseBuilder.h"
#include <iostream>

void MusicScanController::init(std::shared_ptr<MusicDatabase> db,
                               std::shared_ptr<MetadataCache> cache,
                               std::shared_ptr<MusicScanner> scanner) {
  scanner_ = scanner;
  std::cout << "[MusicScanController] Initialized" << std::endl;
}

void MusicScanController::scan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    if (!scanner_) {
      ResponseBuilder::sendError(callback, "Scanner not initialized",
                                 drogon::k500InternalServerError);
      return;
    }
    scanner_->scanNewFiles();
    ResponseBuilder::sendSuccess(callback, Json::Value());
  } catch (const std::exception &e) {
    std::cerr << "[MusicScanController] scan error: " << e.what() << std::endl;
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicScanController::forceRescan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    if (!scanner_) {
      ResponseBuilder::sendError(callback, "Scanner not initialized",
                                 drogon::k500InternalServerError);
      return;
    }
    if (scanner_->isInProgress()) {
      Json::Value response;
      response["status"] = "error";
      response["message"] = "Rescan already in progress";
      response["in_progress"] = true;
      const auto &status = scanner_->getStatus();
      response["total_files"] = status.totalFiles.load();
      response["processed_files"] = status.processedFiles.load();
      callback(ResponseBuilder::jsonResponse(response));
      return;
    }
    scanner_->forceRescan([]() {});
    Json::Value response;
    response["status"] = "success";
    response["message"] = "Force rescan started";
    callback(ResponseBuilder::jsonResponse(response));
  } catch (const std::exception &e) {
    std::cerr << "[MusicScanController] forceRescan error: " << e.what()
              << std::endl;
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicScanController::removeMissing(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    if (!scanner_) {
      ResponseBuilder::sendError(callback, "Scanner not initialized",
                                 drogon::k500InternalServerError);
      return;
    }
    scanner_->removeMissingFiles();
    ResponseBuilder::sendSuccess(callback, Json::Value());
  } catch (const std::exception &e) {
    std::cerr << "[MusicScanController] removeMissing error: " << e.what()
              << std::endl;
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicScanController::getRescanStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    if (!scanner_) {
      ResponseBuilder::sendError(callback, "Scanner not initialized",
                                 drogon::k500InternalServerError);
      return;
    }
    const auto &status = scanner_->getStatus();
    Json::Value response;
    response["status"] = "success";
    response["in_progress"] = status.inProgress.load();
    response["total_files"] = status.totalFiles.load();
    response["processed_files"] = status.processedFiles.load();
    response["added_files"] = status.addedFiles.load();
    response["error_count"] = status.errorCount.load();
    response["old_albums_count"] = status.oldAlbumsCount.load();
    response["new_albums_count"] = status.newAlbumsCount.load();
    if (status.totalFiles.load() > 0) {
      response["percent"] =
          (status.processedFiles.load() * 100) / status.totalFiles.load();
    } else {
      response["percent"] = 0;
    }
    callback(ResponseBuilder::jsonResponse(response));
  } catch (const std::exception &e) {
    std::cerr << "[MusicScanController] getRescanStatus error: " << e.what()
              << std::endl;
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}
