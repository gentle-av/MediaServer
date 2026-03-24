#include "MusicController.h"
#include "services/AlbumArtExtractor.h"
#include "services/MusicMetadataExtractor.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <set>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

MusicController::MusicController() : isScanning_(false), isInitialSync_(false) {
  musicRootPath_ = "/mnt/media/music";
  const char *home = getenv("HOME");
  std::string dbDir;
  if (home) {
    dbDir = std::string(home) + "/.local/share/media-explorer-drogon";
  } else {
    dbDir = "/tmp/media-explorer-drogon";
  }
  std::string dbPath = dbDir + "/music.db";
  mkdir(dbDir.c_str(), 0755);
  db_ = std::make_unique<MusicDatabase>(dbPath);
  if (!db_->initialize()) {
    LOG_ERROR << "Failed to initialize database at " << dbPath;
  } else {
    LOG_INFO << "Database initialized successfully at " << dbPath;
  }
  std::thread([this]() {
    LOG_INFO << "Starting initial music library synchronization...";
    syncMusicLibrary();
    LOG_INFO << "Initial music library synchronization completed";
    auto artists = db_->getArtists();
    LOG_INFO << "Found " << artists.size() << " artists in database";
    auto albums = db_->getAlbums();
    LOG_INFO << "Found " << albums.size() << " albums in database";
  }).detach();
}

MusicController::~MusicController() { stopCurrentPlayback(); }

void MusicController::syncMusicLibrary() {
  std::lock_guard<std::mutex> lock(scanMutex_);
  isScanning_ = true;
  isInitialSync_ = true;
  try {
    if (!fs::exists(musicRootPath_)) {
      LOG_ERROR << "Music root directory does not exist: " << musicRootPath_;
      isScanning_ = false;
      isInitialSync_ = false;
      return;
    }
    if (!fs::is_directory(musicRootPath_)) {
      LOG_ERROR << "Music root path is not a directory: " << musicRootPath_;
      isScanning_ = false;
      isInitialSync_ = false;
      return;
    }
    LOG_INFO << "Starting library synchronization for: " << musicRootPath_;
    removeMissingFiles();
    scanNewFiles();
    LOG_INFO << "Library synchronization completed successfully";
  } catch (const std::exception &e) {
    LOG_ERROR << "Error during library synchronization: " << e.what();
  }
  isScanning_ = false;
  isInitialSync_ = false;
}

void MusicController::removeMissingFiles() {
  LOG_INFO << "Checking for missing files in database...";
  auto allFiles = db_->getAllFiles();
  std::vector<std::string> missingFiles;
  for (const auto &filePath : allFiles) {
    if (!fs::exists(filePath) || !isValidMusicPath(filePath)) {
      missingFiles.push_back(filePath);
    }
  }
  if (!missingFiles.empty()) {
    LOG_INFO << "Found " << missingFiles.size() << " missing files to remove";
    for (const auto &filePath : missingFiles) {
      LOG_INFO << "Removing missing file: " << filePath;
      db_->removeFile(filePath);
    }
  } else {
    LOG_INFO << "No missing files found";
  }
}

void MusicController::scanNewFiles() {
  LOG_INFO << "Scanning for new files...";
  std::set<std::string> existingFiles;
  auto dbFiles = db_->getAllFiles();
  existingFiles.insert(dbFiles.begin(), dbFiles.end());
  int newFilesCount = 0;
  int totalFilesScanned = 0;
  std::function<void(const fs::path &)> traverseAndAdd =
      [&](const fs::path &currentPath) {
        try {
          for (const auto &entry : fs::directory_iterator(currentPath)) {
            if (entry.is_directory()) {
              traverseAndAdd(entry.path());
            } else if (entry.is_regular_file()) {
              std::string ext = entry.path().extension().string();
              std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
              if (isAudioFile(ext)) {
                totalFilesScanned++;
                std::string filePath = entry.path().string();
                if (existingFiles.find(filePath) == existingFiles.end()) {
                  LOG_INFO << "Found new file: " << filePath;
                  auto metadata = MusicMetadataExtractor::extract(filePath);
                  if (db_->addFile(filePath, metadata)) {
                    newFilesCount++;
                    LOG_INFO << "Added new file to database: " << filePath;
                  } else {
                    LOG_ERROR << "Failed to add file to database: " << filePath;
                  }
                  auto albumArt = AlbumArtExtractor::extract(filePath);
                  if (!albumArt.data.empty()) {
                    db_->saveAlbumArt(filePath, albumArt);
                    LOG_INFO << "Saved album art for: " << filePath;
                  }
                }
              }
            }
          }
        } catch (const std::exception &e) {
          LOG_ERROR << "Error scanning directory " << currentPath << ": "
                    << e.what();
        }
      };
  try {
    traverseAndAdd(musicRootPath_);
    LOG_INFO << "Scan completed. Total files scanned: " << totalFilesScanned;
    LOG_INFO << "New files added: " << newFilesCount;
  } catch (const std::exception &e) {
    LOG_ERROR << "Error during file scanning: " << e.what();
  }
}

bool MusicController::isValidMusicPath(const std::string &path) {
  if (path.find(musicRootPath_) != 0) {
    return false;
  }
  if (path.find("..") != std::string::npos) {
    return false;
  }
  fs::path fsPath(path);
  std::string ext = fsPath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return isAudioFile(ext);
}

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
    std::string path = musicRootPath_;
    if (json && json->isMember("path")) {
      path = (*json)["path"].asString();
    }
    if (!isValidMusicPath(path) && path != musicRootPath_) {
      response["success"] = false;
      response["error"] = "Access denied";
      auto resp = HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(k403Forbidden);
      addCorsHeaders(resp);
      callback(resp);
      return;
    }
    if (!fs::exists(path) || !fs::is_directory(path)) {
      response["success"] = false;
      response["error"] = "Directory not found";
      auto resp = HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(k404NotFound);
      addCorsHeaders(resp);
      callback(resp);
      return;
    }
    std::vector<Json::Value> items;
    for (const auto &entry : fs::directory_iterator(path)) {
      if (entry.is_directory()) {
        Json::Value item;
        item["name"] = entry.path().filename().string();
        item["path"] = entry.path().string();
        item["type"] = "directory";
        item["size"] = "";
        items.push_back(item);
      } else if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (isAudioFile(ext)) {
          Json::Value item;
          item["name"] = entry.path().filename().string();
          item["path"] = entry.path().string();
          item["size"] = formatFileSize(entry.file_size());
          item["extension"] = ext;
          item["type"] = "file";
          auto metadata = db_->getMetadata(entry.path().string());
          if (!metadata.artist.empty()) {
            item["artist"] = metadata.artist;
          }
          if (!metadata.album.empty()) {
            item["album"] = metadata.album;
          }
          items.push_back(item);
        }
      }
    }
    std::sort(items.begin(), items.end(),
              [](const Json::Value &a, const Json::Value &b) {
                if (a["type"].asString() != b["type"].asString()) {
                  return a["type"].asString() == "directory";
                }
                return a["name"].asString() < b["name"].asString();
              });
    Json::Value itemsArray(Json::arrayValue);
    for (const auto &item : items) {
      itemsArray.append(item);
    }
    response["items"] = itemsArray;
    response["total"] = (int)items.size();
    response["success"] = true;
    response["currentPath"] = path;
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
  if (!isValidMusicPath(path)) {
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
  if (!isValidMusicPath(path)) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Access denied";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k403Forbidden);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  auto albumArt = db_->getAlbumArt(path);
  if (albumArt.data.empty()) {
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
  resp->addHeader("Content-Type", albumArt.mimeType);
  resp->addHeader("Content-Length", std::to_string(albumArt.data.size()));
  resp->addHeader("Cache-Control", "public, max-age=86400");
  addCorsHeaders(resp);
  resp->setBody(std::string(albumArt.data.data(), albumArt.data.size()));
  callback(resp);
}

void MusicController::getAlbums(
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
    std::string artistFilter = "";
    if (json && json->isMember("artist")) {
      artistFilter = (*json)["artist"].asString();
    }
    auto albums = db_->getAlbums(artistFilter);
    Json::Value albumsArray(Json::arrayValue);
    for (const auto &album : albums) {
      Json::Value albumObj;
      albumObj["artist"] = std::get<0>(album);
      albumObj["album"] = std::get<1>(album);
      albumObj["path"] = std::get<2>(album);
      albumsArray.append(albumObj);
    }
    response["success"] = true;
    response["albums"] = albumsArray;
    response["total"] = (int)albums.size();
    if (!artistFilter.empty()) {
      response["artistFilter"] = artistFilter;
    }
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::getArtists(
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
    auto artists = db_->getArtists();
    Json::Value artistsArray(Json::arrayValue);
    for (const auto &artist : artists) {
      artistsArray.append(artist);
    }
    response["success"] = true;
    response["artists"] = artistsArray;
    response["total"] = (int)artists.size();
  } catch (const std::exception &e) {
    response["success"] = false;
    response["error"] = e.what();
  }
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}

void MusicController::rescanLibrary(
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
  std::lock_guard<std::mutex> lock(scanMutex_);
  if (isScanning_) {
    response["success"] = false;
    response["error"] = "Scan already in progress";
    auto resp = HttpResponse::newHttpJsonResponse(response);
    addCorsHeaders(resp);
    callback(resp);
    return;
  }
  isScanning_ = true;
  std::thread([this]() {
    syncMusicLibrary();
    isScanning_ = false;
  }).detach();
  response["success"] = true;
  response["message"] = "Library rescan started";
  auto resp = HttpResponse::newHttpJsonResponse(response);
  addCorsHeaders(resp);
  callback(resp);
}
