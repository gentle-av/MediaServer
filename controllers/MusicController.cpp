#include "MusicController.h"
#include "services/AlbumArtExtractor.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void MusicController::addCorsHeaders(const HttpResponsePtr &resp) {
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, X-Requested-With");
  resp->addHeader("Access-Control-Allow-Credentials", "true");
}

void MusicController::listFiles(
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
  try {
    auto json = req->getJsonObject();
    std::string path = "/mnt/media/music";
    if (json && json->isMember("path")) {
      path = (*json)["path"].asString();
    }
    if (path.find("/mnt/media/music") != 0) {
      path = "/mnt/media/music";
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
        item["isAudio"] = isAudioFile(ext);
        item["isVideo"] = false;
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
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::openAudio(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  std::string path = (*json)["path"].asString();
  if (path.find("/mnt/media/music") != 0) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  if (!fs::exists(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state.playlist.empty() ||
        std::find(state.playlist.begin(), state.playlist.end(), path) ==
            state.playlist.end()) {
      state.playlist.push_back(path);
      state.currentIndex = state.playlist.size() - 1;
    } else {
      auto it = std::find(state.playlist.begin(), state.playlist.end(), path);
      state.currentIndex = std::distance(state.playlist.begin(), it);
    }
    state.currentFile = path;
  }
  playFile(path);
  Json::Value response;
  response["success"] = true;
  response["message"] = "Opening audio file: " + path;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::play(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state.currentFile.empty() && !state.playlist.empty()) {
      state.currentIndex = 0;
      state.currentFile = state.playlist[0];
      playFile(state.currentFile);
      state.isPlaying = true;
      state.isPaused = false;
    } else if (!state.currentFile.empty() && state.isPaused) {
      std::string cmd = "pkill -CONT -f " + state.currentFile + " 2>/dev/null";
      system(cmd.c_str());
      state.isPlaying = true;
      state.isPaused = false;
    } else if (!state.currentFile.empty() && !state.isPlaying) {
      playFile(state.currentFile);
      state.isPlaying = true;
      state.isPaused = false;
    }
  }
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::pause(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!state.currentFile.empty() && state.isPlaying) {
      std::string cmd = "pkill -STOP -f " + state.currentFile + " 2>/dev/null";
      system(cmd.c_str());
      state.isPlaying = false;
      state.isPaused = true;
    }
  }
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::next(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state.playlist.empty()) {
      response["success"] = false;
      response["error"] = "Playlist is empty";
    } else {
      stopCurrentPlayback();
      state.currentIndex = (state.currentIndex + 1) % state.playlist.size();
      state.currentFile = state.playlist[state.currentIndex];
      playFile(state.currentFile);
      state.isPlaying = true;
      state.isPaused = false;
      response["success"] = true;
    }
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::previous(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state.playlist.empty()) {
      response["success"] = false;
      response["error"] = "Playlist is empty";
    } else {
      stopCurrentPlayback();
      state.currentIndex = (state.currentIndex - 1 + state.playlist.size()) %
                           state.playlist.size();
      state.currentFile = state.playlist[state.currentIndex];
      playFile(state.currentFile);
      state.isPlaying = true;
      state.isPaused = false;
      response["success"] = true;
    }
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::stop(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    stopCurrentPlayback();
    state.isPlaying = false;
    state.isPaused = false;
  }
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::clear(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    stopCurrentPlayback();
    state.playlist.clear();
    state.currentFile.clear();
    state.currentIndex = -1;
    state.isPlaying = false;
    state.isPaused = false;
    state.playerPid.clear();
  }
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::getStatus(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    response["success"] = true;
    response["available"] = true;
    response["isPlaying"] = state.isPlaying;
    response["isPaused"] = state.isPaused;
    response["currentFile"] = state.currentFile;
    response["currentIndex"] = state.currentIndex;
    response["playlistSize"] = (int)state.playlist.size();
    response["volume"] = state.volume;
    response["playerPid"] = state.playerPid;
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::getPlaylist(
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
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    Json::Value playlist(Json::arrayValue);
    for (const auto &file : state.playlist) {
      playlist.append(file);
    }
    response["playlist"] = playlist;
    response["currentIndex"] = state.currentIndex;
    response["success"] = true;
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::addToPlaylist(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  std::string path = (*json)["path"].asString();
  if (!fs::exists(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (std::find(state.playlist.begin(), state.playlist.end(), path) ==
        state.playlist.end()) {
      state.playlist.push_back(path);
    }
  }
  Json::Value response;
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::removeFromPlaylist(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto json = req->getJsonObject();
  if (!json || !json->isMember("index")) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No index provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  int index = (*json)["index"].asInt();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (index >= 0 && index < (int)state.playlist.size()) {
      if (index == state.currentIndex) {
        stopCurrentPlayback();
      }
      state.playlist.erase(state.playlist.begin() + index);
      if (state.currentIndex > index) {
        state.currentIndex--;
      } else if (state.currentIndex == index) {
        state.currentFile.clear();
        state.currentIndex = -1;
        state.isPlaying = false;
        state.isPaused = false;
      }
    }
  }
  Json::Value response;
  response["success"] = true;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

std::string MusicController::getMimeType(const std::string &extension) {
  static std::map<std::string, std::string> mimeTypes = {
      {".mp3", "audio/mpeg"},     {".flac", "audio/flac"},
      {".wav", "audio/wav"},      {".aac", "audio/aac"},
      {".ogg", "audio/ogg"},      {".m4a", "audio/mp4"},
      {".wma", "audio/x-ms-wma"}, {".opus", "audio/opus"}};
  auto it = mimeTypes.find(extension);
  if (it != mimeTypes.end()) {
    return it->second;
  }
  return "application/octet-stream";
}

Json::Value MusicController::getFileInfo(const fs::path &path) {
  Json::Value info;
  info["name"] = path.filename().string();
  info["path"] = path.string();
  info["isDirectory"] = fs::is_directory(path);
  if (fs::is_regular_file(path)) {
    info["size"] = formatFileSize(fs::file_size(path));
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    info["isAudio"] = isAudioFile(ext);
  }
  return info;
}

bool MusicController::isAudioFile(const std::string &filename) {
  std::vector<std::string> audioExts = {".mp3", ".flac", ".wav", ".aac",
                                        ".ogg", ".m4a",  ".wma", ".opus"};
  std::string lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return std::find(audioExts.begin(), audioExts.end(), lower) !=
         audioExts.end();
}

std::string MusicController::formatFileSize(uintmax_t size) {
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

void MusicController::playFile(const std::string &path) {
  stopCurrentPlayback();
  launchPlayerWithFile(path);
}

void MusicController::stopCurrentPlayback() {
  if (!state.playerPid.empty()) {
    std::string cmd = "kill " + state.playerPid + " 2>/dev/null";
    system(cmd.c_str());
    state.playerPid.clear();
  }
  std::string cmd = "pkill -f /usr/bin/vlc.*/mnt/media/music 2>/dev/null; "
                    "pkill -f /usr/bin/mpv.*/mnt/media/music 2>/dev/null; "
                    "pkill -f audacious.*/mnt/media/music 2>/dev/null";
  system(cmd.c_str());
}

void MusicController::launchPlayerWithFile(const std::string &path) {
  std::string scriptPath =
      "/tmp/run_audio_" + std::to_string(time(nullptr)) + ".sh";
  std::string playerCmd = getPlayerCommand(path);
  std::string scriptContent = "#!/bin/bash\n"
                              "export DISPLAY=:0\n"
                              "export XAUTHORITY=/home/avr/.Xauthority\n"
                              "nohup " +
                              playerCmd + " \"" + path +
                              "\" > /tmp/audio_player.log 2>&1 &\n"
                              "echo $!";
  std::ofstream scriptFile(scriptPath);
  if (scriptFile.is_open()) {
    scriptFile << scriptContent;
    scriptFile.close();
    chmod(scriptPath.c_str(), 0755);
    std::string command = "bash " + scriptPath;
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe) {
      char buffer[128];
      if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string pid = buffer;
        pid.erase(std::remove(pid.begin(), pid.end(), '\n'), pid.end());
        std::lock_guard<std::mutex> lock(stateMutex);
        state.playerPid = pid;
      }
      pclose(pipe);
    }
  }
}

std::string MusicController::getPlayerCommand(const std::string &path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".m4a") {
    return "/usr/bin/vlc --play-and-exit";
  } else {
    return "/usr/bin/vlc";
  }
}

void MusicController::getAlbumArt(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto params = req->getParameters();
  if (params.find("path") == params.end()) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No path provided";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  std::string path = params["path"];
  if (path.find("/mnt/media/music") != 0) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  if (!fs::exists(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "File not found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  if (!AlbumArtExtractor::isSupportedFormat(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Format not supported";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k400BadRequest);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto albumArt = AlbumArtExtractor::extractAlbumArt(path);
  if (!albumArt || albumArt->data.empty()) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "No album art found";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k404NotFound);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
  resp->addHeader("Content-Type", albumArt->mimeType);
  resp->addHeader("Content-Length", std::to_string(albumArt->data.size()));
  resp->addHeader("Cache-Control", "public, max-age=86400");
  addCorsHeaders(resp);
  resp->setBody(std::string(albumArt->data.data(), albumArt->data.size()));
  callback(resp);
}
