#include "VideoController.h"
#include "profilers/Profiler.h"
#include "services/PlayerService.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

std::shared_ptr<PlayerService> VideoController::playerService_ = nullptr;

void VideoController::setPlayerService(std::shared_ptr<PlayerService> service) {
  playerService_ = service;
}

void VideoController::getIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string indexPath;
  if (profiler_) {
    indexPath = profiler_->getIndexPath();
  }
  if (indexPath.empty()) {
    std::vector<std::string> searchPaths = {
        "/home/avr/code/html/test/views/index.html",
        "/home/avr/code/html/product/views/index.html", "./views/index.html",
        "./index.html"};
    for (const auto &path : searchPaths) {
      std::ifstream file(path);
      if (file.good()) {
        indexPath = path;
        break;
      }
    }
  }
  if (!indexPath.empty()) {
    std::ifstream file(indexPath);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(content);
    callback(resp);
  } else {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k404NotFound);
    resp->setBody("index.html not found");
    callback(resp);
  }
}

void VideoController::serveStatic(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &filename) {
  std::string basePath;
  if (profiler_) {
    basePath = profiler_->getDocumentRoot();
  }
  std::vector<std::string> searchPaths;
  if (!basePath.empty()) {
    searchPaths.push_back(basePath + "/" + filename);
  }
  searchPaths.push_back("/home/avr/code/html/test/views/" + filename);
  searchPaths.push_back("/home/avr/code/html/product/views/" + filename);
  searchPaths.push_back("./views/" + filename);
  fs::path filePath;
  for (const auto &path : searchPaths) {
    if (fs::exists(path) && fs::is_regular_file(path)) {
      filePath = path;
      break;
    }
  }
  if (!filePath.empty()) {
    auto resp = HttpResponse::newFileResponse(filePath.string());
    callback(resp);
  } else {
    Json::Value json;
    json["error"] = "File not found: " + filename;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}

void VideoController::listFiles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto json = req->getJsonObject();
    std::string path = "/mnt/video";
    if (json && json->isMember("path")) {
      path = (*json)["path"].asString();
    }
    if (path.find("/mnt/video") != 0) {
      path = "/mnt/video";
    }
    std::vector<Json::Value> items;
    for (const auto &entry : fs::directory_iterator(path)) {
      Json::Value item;
      item["name"] = entry.path().filename().string();
      item["path"] = entry.path().string();
      item["isDirectory"] = entry.is_directory();
      if (entry.is_regular_file()) {
        item["size"] = formatFileSize(entry.file_size());
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        item["isVideo"] = isVideoFile(ext);
      }
      items.push_back(item);
    }
    std::sort(items.begin(), items.end(),
              [](const Json::Value &a, const Json::Value &b) {
                bool aIsDir = a["isDirectory"].asBool();
                bool bIsDir = b["isDirectory"].asBool();
                if (aIsDir != bIsDir) {
                  return aIsDir > bIsDir;
                }
                return a["name"].asString() < b["name"].asString();
              });
    Json::Value itemsArray(Json::arrayValue);
    for (const auto &item : items) {
      itemsArray.append(item);
    }
    response["items"] = itemsArray;
    response["success"] = true;
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::openVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::cout << "[DEBUG] openVideo: ENTERED" << std::endl;
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
  if (path.find("/mnt/video") != 0) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }
  if (!fs::exists(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    callback(resp);
    return;
  }
  Json::Value response;
  if (playerService_) {
    std::cout << "[DEBUG] openVideo: calling stopAll()" << std::endl;
    playerService_->stopAll();
    std::cout << "[DEBUG] openVideo: calling clear()" << std::endl;
    playerService_->clear();
    std::cout << "[DEBUG] openVideo: calling setVideoEnabled(true)"
              << std::endl;
    playerService_->setVideoEnabled(true);
    std::cout << "[DEBUG] openVideo: calling replacePlaylistWithTrack"
              << std::endl;
    playerService_->replacePlaylistWithTrack(path);
    response["success"] = true;
    response["message"] = "Video playing with local player";
  } else {
    response["success"] = false;
    response["error"] = "Player service not available";
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

std::string VideoController::getMimeType(const std::string &extension) {
  static std::map<std::string, std::string> mimeTypes = {
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".json", "application/json"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".svg", "image/svg+xml"},
      {".ico", "image/x-icon"},      {".txt", "text/plain"},
      {".mp4", "video/mp4"},         {".webm", "video/webm"},
      {".ogg", "video/ogg"},         {".avi", "video/x-msvideo"},
      {".mkv", "video/x-matroska"}};
  auto it = mimeTypes.find(extension);
  if (it != mimeTypes.end()) {
    return it->second;
  }
  return "application/octet-stream";
}

Json::Value VideoController::getFileInfo(const fs::path &path) {
  Json::Value info;
  info["name"] = path.filename().string();
  info["path"] = path.string();
  info["isDirectory"] = fs::is_directory(path);
  if (fs::is_regular_file(path)) {
    info["size"] = formatFileSize(fs::file_size(path));
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    info["isVideo"] = isVideoFile(ext);
  }
  return info;
}

bool VideoController::isVideoFile(const std::string &filename) {
  std::vector<std::string> videoExts = {".mp4", ".avi", ".mkv",  ".mov",
                                        ".wmv", ".flv", ".webm", ".m4v",
                                        ".mpg", ".mpeg"};
  std::string lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return std::find(videoExts.begin(), videoExts.end(), lower) !=
         videoExts.end();
}

std::string VideoController::formatFileSize(uintmax_t size) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double fileSize = size;
  while (fileSize >= 1024 && unitIndex < 4) {
    fileSize /= 1024;
    unitIndex++;
  }
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.1f %s", fileSize, units[unitIndex]);
  return std::string(buffer);
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
  if (path.find("/mnt/video") != 0 && path.find("/mnt/media/music") != 0) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }
  if (!fs::exists(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    callback(resp);
    return;
  }
  std::string trashCmd = "gio trash \"" + path +
                         "\" 2>/dev/null || trash-put \"" + path +
                         "\" 2>/dev/null";
  int result = system(trashCmd.c_str());
  if (result == 0) {
    Json::Value response;
    response["success"] = true;
    response["message"] = "File moved to trash";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  } else {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Failed to move file to trash";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}
