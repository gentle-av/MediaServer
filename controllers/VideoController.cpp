#include "VideoController.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

void addCorsHeaders(const HttpResponsePtr &resp) {
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, X-Requested-With");
  resp->addHeader("Access-Control-Allow-Credentials", "true");
}

void VideoController::getIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string indexPath =
      "/home/avr/code/projects/cpp/build/MediaServer/views/index.html";
  std::ifstream file(indexPath);
  if (file.good()) {
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
    resp->setBody(
        "index.html not found at"
        "/home/avr/code/projects/cpp/build/MediaServer/views/index.html");
    callback(resp);
  }
}

void VideoController::serveStatic(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &filename) {
  fs::path filePath =
      "/home/avr/code/projects/cpp/build/MediaServer/views/" + filename;
  std::cout << "[serveStatic] Serving: " << filePath << std::endl;
  if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
    auto resp = HttpResponse::newFileResponse(filePath.string());
    callback(resp);
  } else {
    Json::Value json;
    json["error"] = "File not found: " + filePath.string();
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

    // Проверка безопасности - путь должен начинаться с /mnt/video
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

    // Сортируем: сначала папки, потом файлы, по алфавиту
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

  std::cout << "========== openVideo called ==========" << std::endl;

  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    std::cout << "ERROR: No path provided" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  std::string path = (*json)["path"].asString();
  std::cout << "Path: " << path << std::endl;

  if (path.find("/mnt/video") != 0) {
    std::cout << "ERROR: Access denied" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  if (!fs::exists(path)) {
    std::cout << "ERROR: File not found" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    callback(resp);
    return;
  }

  // Создаем скрипт для запуска
  std::string scriptPath =
      "/tmp/run_mediateka_" + std::to_string(time(nullptr)) + ".sh";
  std::cout << "Creating script: " << scriptPath << std::endl;

  std::string scriptContent = "#!/bin/bash\n"
                              "export DISPLAY=:0\n"
                              "export XAUTHORITY=/home/avr/.Xauthority\n"
                              "nohup /usr/local/bin/Mediateka \"" +
                              path +
                              "\" > /tmp/mediateka.log 2>&1 &\n"
                              "exit 0\n";

  std::cout << "Script content:\n" << scriptContent << std::endl;

  std::ofstream scriptFile(scriptPath);
  if (scriptFile.is_open()) {
    scriptFile << scriptContent;
    scriptFile.close();

    chmod(scriptPath.c_str(), 0755);
    std::cout << "Script created and made executable" << std::endl;

    std::string command = "bash " + scriptPath;
    std::cout << "Running command: " << command << std::endl;

    int result = system(command.c_str());
    std::cout << "system() returned: " << result << std::endl;

    Json::Value response;
    response["success"] = true;
    response["message"] = "Opening video with Mediateka: " + path;
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  } else {
    std::cout << "ERROR: Failed to create script" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "Failed to create launch script";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }

  std::cout << "========== openVideo finished ==========" << std::endl;
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

void VideoController::getStatus(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  Json::Value response;
  response["success"] = true;
  response["available"] = true;
  response["isFullScreen"] = false;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void VideoController::setFullscreen(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  Json::Value response;
  auto json = req->getJsonObject();
  bool fullscreen = true;
  if (json && json->isMember("fullscreen")) {
    fullscreen = (*json)["fullscreen"].asBool();
  }
  response["success"] = true;
  response["fullscreen"] = fullscreen;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}
