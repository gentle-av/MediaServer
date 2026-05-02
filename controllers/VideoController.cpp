#include "VideoController.h"
#include "profilers/Profiler.h"
#include "services/ThumbnailExtractor.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <unistd.h>
#include <vector>

extern Profiler *g_profiler;
std::string VideoController::activeSocket_ = "";

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
        item["icon"] = getIconForFile(ext);
      } else {
        item["icon"] = "folder";
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

std::string VideoController::getIconForFile(const std::string &ext) {
  if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" ||
      ext == ".wmv" || ext == ".flv" || ext == ".webm" || ext == ".m4v" ||
      ext == ".mpg" || ext == ".mpeg") {
    return "video";
  }
  if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" ||
      ext == ".bmp" || ext == ".svg") {
    return "image";
  }
  if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
      ext == ".m4a") {
    return "audio";
  }
  if (ext == ".pdf") {
    return "pdf";
  }
  if (ext == ".txt" || ext == ".md" || ext == ".log") {
    return "text";
  }
  return "file";
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
  Json::Value response;
  response["success"] = false;
  response["error"] = "Thumbnails disabled";
  response["use_icon"] = true;
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
  std::cout << "=== getPlaybackStatus called ===" << std::endl;
  Json::Value response;
  if (activeSocket_.empty()) {
    std::cout << "No active socket" << std::endl;
    response["success"] = true;
    response["playing"] = false;
    response["reason"] = "no_active_video";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::cout << "Active socket: " << activeSocket_ << std::endl;
  std::string checkCmd =
      "kill -0 $(pgrep -f '" + activeSocket_ + "') 2>/dev/null";
  int result = system(checkCmd.c_str());
  if (result != 0) {
    std::cout << "Process is dead" << std::endl;
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
      activeSocket_ + " 2>&1";
  std::cout << "Pause command: " << cmd << std::endl;
  std::array<char, 128> buffer;
  std::string result_str;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::cout << "Failed to execute command" << std::endl;
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
  std::cout << "Pause response: " << result_str << std::endl;
  bool isPaused = result_str.find("\"data\":true") != std::string::npos;
  std::cout << "Is paused: " << (isPaused ? "true" : "false") << std::endl;
  cmd = "echo '{ \"command\": [\"get_property\", \"time-pos\"] }' | socat - " +
        activeSocket_ + " 2>&1";
  std::cout << "Time command: " << cmd << std::endl;
  pipe = popen(cmd.c_str(), "r");
  double currentTime = 0;
  if (pipe) {
    result_str.clear();
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    std::cout << "Time response: " << result_str << std::endl;
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find(":", pos);
      if (start != std::string::npos) {
        currentTime = std::stod(result_str.substr(start + 1));
        std::cout << "Current time: " << currentTime << std::endl;
      }
    }
  }
  cmd = "echo '{ \"command\": [\"get_property\", \"duration\"] }' | socat - " +
        activeSocket_ + " 2>&1";
  std::cout << "Duration command: " << cmd << std::endl;
  pipe = popen(cmd.c_str(), "r");
  double duration = 0;
  if (pipe) {
    result_str.clear();
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    std::cout << "Duration response: " << result_str << std::endl;
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find(":", pos);
      if (start != std::string::npos) {
        duration = std::stod(result_str.substr(start + 1));
        std::cout << "Duration: " << duration << std::endl;
      }
    }
  }
  cmd = "echo '{ \"command\": [\"get_property\", \"path\"] }' | socat - " +
        activeSocket_ + " 2>&1";
  std::cout << "Path command: " << cmd << std::endl;
  pipe = popen(cmd.c_str(), "r");
  std::string currentFile;
  if (pipe) {
    result_str.clear();
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    std::cout << "Path response: " << result_str << std::endl;
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find("\"", pos + 7);
      if (start != std::string::npos) {
        size_t end = result_str.find("\"", start + 1);
        if (end != std::string::npos) {
          currentFile = result_str.substr(start + 1, end - start - 1);
          std::cout << "Current file: " << currentFile << std::endl;
        }
      }
    }
  }
  response["success"] = true;
  response["playing"] = true;
  response["paused"] = isPaused;
  response["currentTime"] = currentTime;
  response["duration"] = duration;
  response["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  response["currentFile"] = currentFile;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::controlMpv(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::cout << "=== controlMpv called ===" << std::endl;
  auto json = req->getJsonObject();
  if (!json || !json->isMember("command")) {
    std::cout << "ERROR: Missing command parameter" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing command parameter";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  std::string command = (*json)["command"].asString();
  std::cout << "Command received: " << command << std::endl;
  if (activeSocket_.empty()) {
    std::cout << "ERROR: activeSocket_ is empty" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "No active video playing";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::cout << "Active socket: " << activeSocket_ << std::endl;
  std::string checkCmd = "pgrep -f '" + activeSocket_ + "'";
  std::cout << "Check command: " << checkCmd << std::endl;
  std::array<char, 128> buffer;
  std::string result_str;
  FILE *pipe = popen(checkCmd.c_str(), "r");
  bool processAlive = false;
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    processAlive = !result_str.empty();
  }
  std::cout << "Process alive: " << (processAlive ? "yes" : "no") << std::endl;
  if (!processAlive) {
    std::cout << "MPV process is dead, clearing socket" << std::endl;
    activeSocket_.clear();
    Json::Value response;
    response["success"] = false;
    response["error"] = "MPV process is dead";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string mpvCommand;
  std::string jsonCommand;
  if (command == "play") {
    jsonCommand = "{\"command\": [\"set_property\", \"pause\", false]}";
  } else if (command == "pause") {
    jsonCommand = "{\"command\": [\"set_property\", \"pause\", true]}";
  } else if (command == "cycle pause") {
    jsonCommand = "{\"command\": [\"cycle\", \"pause\"]}";
  } else if (command == "stop") {
    jsonCommand = "{\"command\": [\"quit\"]}";
  } else if (command == "fullscreen") {
    jsonCommand = "{\"command\": [\"cycle\", \"fullscreen\"]}";
  } else {
    jsonCommand = "{\"command\": [\"" + command + "\"]}";
  }
  std::cout << "JSON command: " << jsonCommand << std::endl;
  std::string cmd =
      "echo '" + jsonCommand + "' | socat - " + activeSocket_ + " 2>&1";
  std::cout << "Full command: " << cmd << std::endl;
  result_str.clear();
  pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
  }
  std::cout << "Socat response: " << result_str << std::endl;
  Json::Value response;
  response["success"] = true;
  response["command_sent"] = command;
  response["socat_response"] = result_str;
  if (command == "stop") {
    activeSocket_.clear();
    response["message"] = "Video stopped";
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::seekMpv(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::cout << "=== seekMpv called ===" << std::endl;
  auto json = req->getJsonObject();
  if (!json) {
    std::cout << "No JSON in request" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "Invalid JSON";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  if (!json->isMember("time")) {
    std::cout << "Missing 'time' parameter" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "Missing 'time' parameter (seconds)";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  double seekTime = (*json)["time"].asDouble();
  std::cout << "Seek time: " << seekTime << " seconds" << std::endl;
  if (activeSocket_.empty()) {
    std::cout << "activeSocket_ is empty" << std::endl;
    Json::Value response;
    response["success"] = false;
    response["error"] = "No active video playing";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::cout << "Active socket: " << activeSocket_ << std::endl;
  std::string checkCmd = "pgrep -f '" + activeSocket_ + "'";
  std::cout << "Check command: " << checkCmd << std::endl;
  std::array<char, 128> buffer;
  std::string result_str;
  FILE *pipe = popen(checkCmd.c_str(), "r");
  bool processAlive = false;
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    processAlive = !result_str.empty();
  }
  std::cout << "Process alive: " << (processAlive ? "yes" : "no") << std::endl;
  if (!processAlive) {
    std::cout << "MPV process is dead, clearing socket" << std::endl;
    activeSocket_.clear();
    Json::Value response;
    response["success"] = false;
    response["error"] = "MPV process is dead";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string durationCmd =
      "echo '{ \"command\": [\"get_property\", \"duration\"] }' | socat - " +
      activeSocket_ + " 2>&1";
  std::cout << "Duration command: " << durationCmd << std::endl;
  result_str.clear();
  pipe = popen(durationCmd.c_str(), "r");
  double duration = 0;
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    std::cout << "Duration response: " << result_str << std::endl;
    size_t pos = result_str.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = result_str.find(":", pos);
      if (start != std::string::npos) {
        duration = std::stod(result_str.substr(start + 1));
        std::cout << "Duration parsed: " << duration << std::endl;
      }
    }
  }
  if (duration > 0) {
    if (seekTime < 0)
      seekTime = 0;
    if (seekTime > duration)
      seekTime = duration;
    std::cout << "Adjusted seek time: " << seekTime << std::endl;
  }
  std::string seekCommand =
      "echo '{\"command\":[\"seek\", " + std::to_string(seekTime) +
      ", \"absolute\"]}' | socat - " + activeSocket_ + " 2>&1";
  std::cout << "Seek command: " << seekCommand << std::endl;
  pipe = popen(seekCommand.c_str(), "r");
  result_str.clear();
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result_str += buffer.data();
    }
    pclose(pipe);
    std::cout << "Seek response: " << result_str << std::endl;
  }
  Json::Value response;
  response["success"] = true;
  response["time"] = seekTime;
  response["duration"] = duration;
  response["debug_socket"] = activeSocket_;
  response["debug_process_alive"] = processAlive;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::getMpvProperty(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &propertyName) {
  Json::Value response;
  if (activeSocket_.empty()) {
    response["success"] = false;
    response["error"] = "No active video playing";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string cmd = "echo '{\"command\": [\"get_property\", \"" + propertyName +
                    "\"]}' | socat - " + activeSocket_ + " 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result_str;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    response["success"] = false;
    response["error"] = "Failed to get property";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result_str += buffer.data();
  }
  pclose(pipe);
  response["success"] = true;
  response["property"] = propertyName;
  response["value"] = result_str;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::closeVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  Json::Value response;
  std::cout << "=== closeVideo: activeSocket_ = '" << activeSocket_ << "'"
            << std::endl;
  if (!activeSocket_.empty()) {
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          activeSocket_ + " 2>/dev/null";
    std::cout << "Quit command: " << quitCmd << std::endl;
    int ret = system(quitCmd.c_str());
    std::cout << "Quit result: " << ret << std::endl;
    activeSocket_.clear();
    response["success"] = true;
    response["message"] = "Video closed and socket cleared";
  } else {
    response["success"] = true;
    response["message"] = "No active video to close";
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::forceStop() {
  std::cout << "=== forceStop called ===" << std::endl;
  system("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  if (!activeSocket_.empty()) {
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          activeSocket_ + " 2>/dev/null";
    system(quitCmd.c_str());
    activeSocket_.clear();
  }
}

void VideoController::forceStopVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::cout << "=== forceStopVideo called ===" << std::endl;
  forceStop();
  Json::Value response;
  response["success"] = true;
  response["message"] = "Video force stopped";
  auto resp = HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void VideoController::openVideo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  std::cout << "=== openVideo called ===" << std::endl;
  system("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  forceStop();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
