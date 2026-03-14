#include "VideoController.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

void VideoController::getIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Путь к index.html
  std::string indexPath = "/usr/local/web/media-explorer/index.html";

  // Проверяем существование файла
  std::ifstream file(indexPath);
  if (file.good()) {
    // Читаем содержимое файла
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(content);
    callback(resp);
  } else {
    // Если файл не найден, возвращаем ошибку
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
  fs::path filePath = "/usr/local/web/media-explorer/" + filename;

  if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
    auto resp = HttpResponse::newFileResponse(filePath.string());
    callback(resp);
  } else {
    Json::Value json;
    json["error"] = "File not found";
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
  Json::Value response;

  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  std::string path = (*json)["path"].asString();

  if (path.find("/mnt/video") != 0) {
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  if (!fs::exists(path)) {
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    callback(resp);
    return;
  }

  // Создаем временный скрипт для запуска
  std::string scriptPath =
      "/tmp/run_mediateka_" + std::to_string(time(nullptr)) + ".sh";

  // Формируем содержимое скрипта (точно как в тестовом)
  std::string scriptContent = "#!/bin/bash\n"
                              "export XDG_RUNTIME_DIR=/run/user/1000\n"
                              "export WAYLAND_DISPLAY=wayland-0\n"
                              "export DISPLAY=:0\n"
                              "sudo -u avr /usr/local/bin/Mediateka \"" +
                              path +
                              "\" &\n"
                              "exit 0\n";

  // Записываем скрипт в файл
  std::ofstream scriptFile(scriptPath);
  if (scriptFile.is_open()) {
    scriptFile << scriptContent;
    scriptFile.close();

    // Делаем скрипт исполняемым
    chmod(scriptPath.c_str(), 0755);

    // Запускаем скрипт в фоне
    std::string command = scriptPath + " > /dev/null 2>&1 &";
    system(command.c_str());

    response["success"] = true;
    response["message"] = "Opening video with Mediateka: " + path;
  } else {
    response["success"] = false;
    response["error"] = "Failed to create launch script";
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
