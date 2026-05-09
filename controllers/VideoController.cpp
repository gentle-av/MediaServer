#include "VideoController.h"
#include "profilers/Profiler.h"
#include "services/video/FileSystemService.h"
#include "services/video/PlaybackService.h"
#include "services/video/PlaybackStatus.h"
#include "services/video/StaticFileService.h"
#include "services/video/ThumbnailRequestHandler.h"
#include "services/video/ThumbnailService.h"
#include "services/video/TrashHandler.h"
#include "services/video/VideoControlHandler.h"

extern Profiler *g_profiler;
std::string VideoController::activeSocket_ = "";

void VideoController::getIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (!profiler_)
    profiler_ = g_profiler;
  auto resp =
      StaticFileService::getInstance().serveIndex(profiler_->getIndexPath());
  callback(resp);
}

void VideoController::serveStatic(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &filename) {
  if (!profiler_)
    profiler_ = g_profiler;
  auto resp = StaticFileService::getInstance().serveStaticFile(
      profiler_->getDocumentRoot(), filename);
  callback(resp);
}

void VideoController::listFiles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &fsService = FileSystemService::getInstance();
  std::string requestPath = "/mnt/video";
  auto json = req->getJsonObject();
  if (json && json->isMember("path") && (*json)["path"].isString()) {
    requestPath = (*json)["path"].asString();
    std::cout << "[DEBUG] listFiles received path from JSON: " << requestPath
              << std::endl;
  } else {
    std::string pathParam = req->getParameter("path");
    if (!pathParam.empty()) {
      requestPath = drogon::utils::urlDecode(pathParam);
      std::cout << "[DEBUG] listFiles received path from param: " << requestPath
                << std::endl;
    }
  }
  if (!fsService.isPathAllowed(requestPath)) {
    std::cout << "[DEBUG] Path not allowed: " << requestPath << std::endl;
    requestPath = "/mnt/video";
  }
  std::cout << "[DEBUG] listFiles final path: " << requestPath << std::endl;
  if (!fsService.fileExists(requestPath) ||
      !fsService.isDirectory(requestPath)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Directory not found: " + requestPath;
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    callback(resp);
    return;
  }
  Json::Value result = fsService.listDirectory(requestPath);
  std::cout << "[DEBUG] listFiles returning result for path: " << requestPath
            << std::endl;
  auto resp = HttpResponse::newHttpJsonResponse(result);
  callback(resp);
}

void VideoController::moveToTrash(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string path = (*json)["path"].asString();
  auto response = TrashHandler::getInstance().handleMoveToTrash(path);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  if (!response["success"].asBool() && response.isMember("error")) {
    if (response["error"].asString().find("Access denied") !=
        std::string::npos) {
      resp->setStatusCode(k403Forbidden);
    } else if (response["error"].asString().find("not found") !=
               std::string::npos) {
      resp->setStatusCode(k404NotFound);
    } else if (response["error"].asString().find("Cannot delete") !=
               std::string::npos) {
      resp->setStatusCode(k400BadRequest);
    } else {
      resp->setStatusCode(k500InternalServerError);
    }
  }
  callback(resp);
}

void VideoController::getThumbnail(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string videoPath = req->getParameter("path");
  int width = 320;
  int quality = 85;
  if (req->getParameter("width") != "") {
    try {
      width = std::stoi(req->getParameter("width"));
      if (width < 50)
        width = 50;
      if (width > 1920)
        width = 1920;
    } catch (...) {
    }
  }
  if (req->getParameter("quality") != "") {
    try {
      quality = std::stoi(req->getParameter("quality"));
      if (quality < 1)
        quality = 1;
      if (quality > 100)
        quality = 100;
    } catch (...) {
    }
  }
  std::string decodedPath = drogon::utils::urlDecode(videoPath);
  auto response = ThumbnailRequestHandler::getInstance().handleSingleRequest(
      decodedPath, width, quality);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  if (!response["success"].asBool()) {
    if (response.isMember("error") &&
        response["error"].asString().find("Access denied") !=
            std::string::npos) {
      resp->setStatusCode(k403Forbidden);
    } else if (response.isMember("error") &&
               response["error"].asString().find("not found") !=
                   std::string::npos) {
      resp->setStatusCode(k404NotFound);
    } else {
      resp->setStatusCode(k400BadRequest);
    }
  }
  callback(resp);
}

void VideoController::getThumbnailsBatch(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("paths") || !(*json)["paths"].isArray()) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing 'paths' array parameter";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  int width = json->isMember("width") ? (*json)["width"].asInt() : 320;
  int quality = json->isMember("quality") ? (*json)["quality"].asInt() : 85;
  std::vector<std::string> paths;
  for (const auto &path : (*json)["paths"]) {
    if (path.isString()) {
      paths.push_back(drogon::utils::urlDecode(path.asString()));
    }
  }
  auto response = ThumbnailRequestHandler::getInstance().handleBatchRequest(
      paths, width, quality);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::clearThumbnailCache(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  ThumbnailService::getInstance().clearCache();
  Json::Value response;
  response["success"] = true;
  response["message"] = "Thumbnail cache cleared";
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::getPlaybackStatus(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto response = PlaybackStatus::getInstance().getStatus(activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::controlMpv(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("command")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing command parameter";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string command = (*json)["command"].asString();
  auto response =
      VideoControlHandler::getInstance().handleControl(command, activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::seekMpv(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Invalid JSON";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  if (!json->isMember("time")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing 'time' parameter (seconds)";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  double seekTime = (*json)["time"].asDouble();
  auto response =
      VideoControlHandler::getInstance().handleSeek(seekTime, activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::getMpvProperty(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &propertyName) {
  auto response = VideoControlHandler::getInstance().handleGetProperty(
      propertyName, activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::closeVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto response = VideoControlHandler::getInstance().handleClose(activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::forceStopVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto response =
      VideoControlHandler::getInstance().handleForceStop(activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::openVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string path = (*json)["path"].asString();
  auto response =
      VideoControlHandler::getInstance().handleOpen(path, activeSocket_);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::deleteDirectory(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string path = (*json)["path"].asString();
  auto response = TrashHandler::getInstance().handleDeleteDirectory(path);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::getThumbnailPost(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing path parameter";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string videoPath = (*json)["path"].asString();
  int width = json->isMember("width") ? (*json)["width"].asInt() : 320;
  int quality = json->isMember("quality") ? (*json)["quality"].asInt() : 85;

  if (width < 50)
    width = 50;
  if (width > 1920)
    width = 1920;
  if (quality < 1)
    quality = 1;
  if (quality > 100)
    quality = 100;
  auto &thumbnailService = ThumbnailService::getInstance();
  auto response =
      thumbnailService.generateThumbnailResponse(videoPath, width, quality);
  auto resp = HttpResponse::newHttpJsonResponse(response);
  if (!response["success"].asBool() && response.isMember("error")) {
    if (response["error"].asString() == "Access denied") {
      resp->setStatusCode(k403Forbidden);
    }
  }
  callback(resp);
}
