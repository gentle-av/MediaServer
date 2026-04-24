#include "VideoController.h"
#include "profilers/Profiler.h"
#include "services/PlayerService.h"
#include "services/ThumbnailExtractor.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <unistd.h>
#include <vector>

std::shared_ptr<PlayerService> VideoController::playerService_ = nullptr;
extern Profiler *g_profiler;

void VideoController::setPlayerService(std::shared_ptr<PlayerService> service) {
  playerService_ = service;
}

void VideoController::getIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (!profiler_) {
    profiler_ = g_profiler;
  }
  std::string indexPath = profiler_->getIndexPath();
  if (indexPath.empty()) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k404NotFound);
    resp->setBody("index.html not found in configuration");
    callback(resp);
    return;
  }
  std::ifstream file(indexPath);
  if (!file.is_open()) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k404NotFound);
    resp->setBody("Cannot open index.html at: " + indexPath);
    callback(resp);
    return;
  }
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->setContentTypeCode(CT_TEXT_HTML);
  resp->setBody(content);
  callback(resp);
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
  static int socketCounter = 0;
  activeSocket_ = "/tmp/mpv-socket-" + std::to_string(getpid()) + "-" +
                  std::to_string(socketCounter++);
  std::string cmd =
      "mpv --fs --vo=gpu-next --hwdec=auto-safe --input-ipc-server=" +
      activeSocket_ + " \"" + path + "\" > /dev/null 2>&1 &";
  int result = system(cmd.c_str());
  Json::Value response;
  response["success"] = (result == 0);
  response["socket"] = activeSocket_;
  response["message"] = (result == 0) ? "Video playing" : "Failed to start mpv";
  auto resp = HttpResponse::newHttpJsonResponse(response);
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
  std::string trashCmd = "kioclient5 move \"" + path + "\" trash:/";
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

void VideoController::getThumbnail(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto pathParam = req->getParameter("path");
  if (pathParam.empty()) {
    Json::Value resp;
    resp["success"] = false;
    resp["error"] = "Missing 'path' parameter";
    auto httpResp = HttpResponse::newHttpJsonResponse(resp);
    httpResp->setStatusCode(k400BadRequest);
    callback(httpResp);
    return;
  }
  if (pathParam.find("/mnt/video") != 0) {
    Json::Value resp;
    resp["success"] = false;
    resp["error"] = "Access denied";
    auto httpResp = HttpResponse::newHttpJsonResponse(resp);
    httpResp->setStatusCode(k403Forbidden);
    callback(httpResp);
    return;
  }
  if (!fs::exists(pathParam) ||
      !isVideoFile(fs::path(pathParam).extension().string())) {
    Json::Value resp;
    resp["success"] = false;
    resp["error"] = "Invalid video file";
    auto httpResp = HttpResponse::newHttpJsonResponse(resp);
    httpResp->setStatusCode(k404NotFound);
    callback(httpResp);
    return;
  }
  int width = 320;
  int quality = 85;
  auto widthParam = req->getParameter("width");
  if (!widthParam.empty()) {
    width = std::stoi(widthParam);
  }
  auto qualityParam = req->getParameter("quality");
  if (!qualityParam.empty()) {
    quality = std::stoi(qualityParam);
  }
  std::string base64Image =
      ThumbnailExtractor::generateThumbnailBase64(pathParam, width, quality);
  if (base64Image.empty()) {
    Json::Value resp;
    resp["success"] = false;
    resp["error"] = "Failed to generate thumbnail";
    auto httpResp = HttpResponse::newHttpJsonResponse(resp);
    httpResp->setStatusCode(k500InternalServerError);
    callback(httpResp);
    return;
  }
  Json::Value response;
  response["success"] = true;
  response["path"] = pathParam;
  response["thumbnail"] = "data:image/jpeg;base64," + base64Image;
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
      {".ico", "image/x-icon"},      {".txt", "text/plain"}};
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

void VideoController::getPlaybackStatus(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  Json::Value response;
  if (activeSocket_.empty()) {
    response["success"] = true;
    response["playing"] = false;
    response["reason"] = "no_active_video";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string checkCmd =
      "kill -0 $(pgrep -f '" + activeSocket_ + "') 2>/dev/null";
  int result = system(checkCmd.c_str());
  if (result != 0) {
    activeSocket_.clear();
    response["success"] = true;
    response["playing"] = false;
    response["reason"] = "process_dead";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string cmd =
      "echo '{ \"command\": [\"get_property\", \"pause\"] }' | socat - " +
      activeSocket_ + " 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result_str;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    response["success"] = false;
    response["error"] = "Failed to check playback status";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result_str += buffer.data();
  }
  pclose(pipe);
  bool isPaused = result_str.find("\"data\":true") != std::string::npos;
  cmd = "echo '{ \"command\": [\"get_property\", \"time-pos\"] }' | socat - " +
        activeSocket_ + " 2>/dev/null";
  pipe = popen(cmd.c_str(), "r");
  double currentTime = 0;
  if (pipe) {
    result_str.clear();
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find(":", pos);
      if (start != std::string::npos) {
        currentTime = std::stod(result_str.substr(start + 1));
      }
    }
  }
  cmd = "echo '{ \"command\": [\"get_property\", \"duration\"] }' | socat - " +
        activeSocket_ + " 2>/dev/null";
  pipe = popen(cmd.c_str(), "r");
  double duration = 0;
  if (pipe) {
    result_str.clear();
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find(":", pos);
      if (start != std::string::npos) {
        duration = std::stod(result_str.substr(start + 1));
      }
    }
  }
  response["success"] = true;
  response["playing"] = true;
  response["paused"] = isPaused;
  response["currentTime"] = currentTime;
  response["duration"] = duration;
  response["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}
