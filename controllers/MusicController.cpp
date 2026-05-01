#include "controllers/MusicController.h"
#include "services/AlbumArtExtractor.h"
#include "tagger/TagEditor.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <iostream>
#include <json/json.h>
#include <regex>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <unordered_set>

namespace fs = std::filesystem;

std::shared_ptr<PlayerController> MusicController::playerController_ = nullptr;
MusicController::RescanStatus MusicController::rescanStatus_;
std::mutex MusicController::rescanStatusMutex_;

void MusicController::openMusium(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto json = req->getJsonObject();
    if (!json || !json->isMember("tracks") || !(*json)["tracks"].isArray()) {
      response["status"] = "error";
      response["message"] = "Missing tracks array parameter";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    std::vector<std::string> tracks;
    for (const auto &track : (*json)["tracks"]) {
      if (track.isString()) {
        tracks.push_back(track.asString());
      }
    }
    if (tracks.empty()) {
      response["status"] = "error";
      response["message"] = "No tracks provided";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    LOG_INFO << "Opening Musium with " << tracks.size() << " tracks";
    if (playerController_) {
      Json::Value playlistJson;
      for (const auto &track : tracks) {
        playlistJson.append(track);
      }
      Json::Value setPlaylistReq;
      setPlaylistReq["tracks"] = playlistJson;
      auto mockReq = drogon::HttpRequest::newHttpJsonRequest(setPlaylistReq);
      playerController_->handleNewSetPlaylist(
          mockReq, [](const drogon::HttpResponsePtr &) {});
      playerController_->handleNewPlay(mockReq,
                                       [](const drogon::HttpResponsePtr &) {});
      response["status"] = "success";
      response["message"] = "Musium launched via PlayerController";
      response["tracks_count"] = static_cast<int>(tracks.size());
    } else {
      response["status"] = "error";
      response["message"] = "PlayerController not available";
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error launching Musium: " << e.what();
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

MusicController::MusicController() {
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  fs::create_directories(fs::path(dbPath).parent_path());
  db_ = std::make_unique<MusicDatabase>(dbPath);
  db_->init();
  musicDir_ = "/mnt/media/music";
  if (!fs::exists(musicDir_))
    musicDir_ = "./music";
}

MusicMetadata *
MusicController::getMetadataFromCache(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  auto it = metadataCache_.find(filePath);
  if (it != metadataCache_.end()) {
    it->second.lastAccess = std::chrono::steady_clock::now();
    return &it->second.metadata;
  }
  return nullptr;
}

void MusicController::addMetadataToCache(const std::string &filePath,
                                         const MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  if (metadataCache_.size() >= MAX_CACHE_SIZE) {
    cleanupCache();
  }
  CachedMetadata cached;
  cached.metadata = metadata;
  cached.lastAccess = std::chrono::steady_clock::now();
  metadataCache_[filePath] = cached;
}

void MusicController::cleanupCache() {
  auto now = std::chrono::steady_clock::now();
  for (auto it = metadataCache_.begin(); it != metadataCache_.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.lastAccess)
                   .count();
    if (age > 3600) {
      it = metadataCache_.erase(it);
    } else {
      ++it;
    }
  }
  if (metadataCache_.size() > MAX_CACHE_SIZE) {
    size_t toErase = metadataCache_.size() - MAX_CACHE_SIZE;
    auto it = metadataCache_.begin();
    for (size_t i = 0; i < toErase && it != metadataCache_.end(); ++i) {
      it = metadataCache_.erase(it);
    }
  }
}

bool MusicController::extractMetadata(const std::string &filePath,
                                      MusicMetadata &metadata) {
  MusicMetadata *cached = getMetadataFromCache(filePath);
  if (cached) {
    metadata = *cached;
    return true;
  }
  if (!fs::exists(filePath)) {
    LOG_ERROR << "File does not exist: " << filePath;
    return false;
  }
  bool success = false;
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      metadata.title = fixTagLibString(tag->title());
      metadata.artist = fixTagLibString(tag->artist());
      metadata.album = fixTagLibString(tag->album());
      metadata.genre = fixTagLibString(tag->genre());
      if (metadata.title.empty()) {
        std::string filename = fs::path(filePath).stem().string();
        std::regex trackPrefix(R"(^\s*\d{1,3}[\.\-\s]+\s*)");
        metadata.title = std::regex_replace(filename, trackPrefix, "");
        if (metadata.title.find_last_of('.') != std::string::npos) {
          metadata.title =
              metadata.title.substr(0, metadata.title.find_last_of('.'));
        }
      }
      if (metadata.title.empty())
        metadata.title = "Unknown";
      if (metadata.artist.empty())
        metadata.artist = "Unknown";
      if (metadata.album.empty())
        metadata.album = "Unknown";
      if (f.audioProperties()) {
        metadata.duration = f.audioProperties()->lengthInSeconds();
      } else {
        metadata.duration = 0;
      }
      metadata.track = tag->track();
      metadata.year = tag->year();
      success = true;
      LOG_INFO << "Extracted: " << filePath << " | " << metadata.artist << " - "
               << metadata.title;
    } else {
      LOG_ERROR << "Cannot read tags from: " << filePath;
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting metadata: " << e.what();
  }
  if (!success) {
    std::string filename = fs::path(filePath).stem().string();
    std::regex trackPrefix(R"(^\s*\d{1,3}[\.\-\s]+\s*)");
    filename = std::regex_replace(filename, trackPrefix, "");
    metadata.title = filename.empty() ? "Unknown" : filename;
    metadata.artist = "Unknown";
    metadata.album = "Unknown";
    metadata.duration = 0;
    metadata.track = 0;
    metadata.year = 0;
    success = true;
  }
  if (success) {
    addMetadataToCache(filePath, metadata);
  }
  return success;
}

void MusicController::refreshFileMetadata(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    response["status"] = "error";
    response["message"] = "Parameter 'path' is required in JSON body";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    if (!fs::exists(decodedPath)) {
      response["status"] = "error";
      response["message"] = "File does not exist";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    MusicMetadata metadata;
    if (extractMetadata(decodedPath, metadata)) {
      if (db_->addFile(decodedPath, metadata)) {
        LOG_INFO << "Refreshed metadata for: " << decodedPath;
        std::vector<char> albumArt;
        if (extractAlbumArt(decodedPath, albumArt)) {
          db_->saveAlbumArt(decodedPath, albumArt);
        }
        {
          std::lock_guard<std::mutex> lock(cacheMutex_);
          metadataCache_.erase(decodedPath);
        }
        addMetadataToCache(decodedPath, metadata);
        response["status"] = "success";
        response["message"] = "Metadata refreshed";
        Json::Value dataObj;
        dataObj["path"] = decodedPath;
        dataObj["title"] = metadata.title;
        dataObj["artist"] = metadata.artist;
        dataObj["album"] = metadata.album;
        dataObj["track"] = metadata.track;
        response["data"] = dataObj;
      } else {
        response["status"] = "error";
        response["message"] = "Failed to save metadata to database";
      }
    } else {
      response["status"] = "error";
      response["message"] = "Failed to extract metadata from file";
    }
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::updateFileTags(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  auto json = req->getJsonObject();
  if (!json) {
    response["status"] = "error";
    response["message"] = "Invalid JSON body";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  if (!json->isMember("path")) {
    response["status"] = "error";
    response["message"] = "Parameter 'path' is required";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    if (!fs::exists(decodedPath)) {
      response["status"] = "error";
      response["message"] = "File does not exist: " + decodedPath;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    MusicMetadata newMetadata;
    if (json->isMember("title")) {
      newMetadata.title = (*json)["title"].asString();
    }
    if (json->isMember("artist")) {
      newMetadata.artist = (*json)["artist"].asString();
    }
    if (json->isMember("album")) {
      newMetadata.album = (*json)["album"].asString();
    }
    if (json->isMember("genre")) {
      newMetadata.genre = (*json)["genre"].asString();
    }
    if (json->isMember("track")) {
      newMetadata.track = (*json)["track"].asInt();
    }
    if (json->isMember("year")) {
      newMetadata.year = (*json)["year"].asInt();
    }
    bool tagsUpdated = updateFileTagsInternal(decodedPath, newMetadata);
    if (!tagsUpdated) {
      response["status"] = "error";
      response["message"] =
          "Failed to update tags in file. Only FLAC files are supported.";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k500InternalServerError);
      callback(resp);
      return;
    }
    MusicMetadata updatedMetadata;
    if (extractMetadata(decodedPath, updatedMetadata)) {
      db_->addFile(decodedPath, updatedMetadata);
      {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        metadataCache_.erase(decodedPath);
      }
      addMetadataToCache(decodedPath, updatedMetadata);
      if (json->isMember("album")) {
        std::vector<char> albumArt;
        if (extractAlbumArt(decodedPath, albumArt)) {
          db_->saveAlbumArt(decodedPath, albumArt);
        }
      }
    }
    response["status"] = "success";
    response["message"] = "Tags updated successfully";
    Json::Value dataObj;
    dataObj["path"] = decodedPath;
    dataObj["title"] = newMetadata.title;
    dataObj["artist"] = newMetadata.artist;
    dataObj["album"] = newMetadata.album;
    dataObj["track"] = newMetadata.track;
    dataObj["year"] = newMetadata.year;
    dataObj["genre"] = newMetadata.genre;
    response["data"] = dataObj;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::listFiles(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto allFiles = db_->getAllFiles();
    Json::Value files(Json::arrayValue);
    for (const auto &filePath : allFiles) {
      if (fs::exists(filePath)) {
        Json::Value fileInfo;
        fileInfo["path"] = filePath;
        fileInfo["filename"] = fs::path(filePath).filename().string();
        MusicMetadata metadata;
        if (db_->getMetadata(filePath, metadata)) {
          fileInfo["title"] = metadata.title;
          fileInfo["artist"] = metadata.artist;
          fileInfo["album"] = metadata.album;
          fileInfo["duration"] = metadata.duration;
          fileInfo["track"] = metadata.track;
          fileInfo["year"] = metadata.year;
          fileInfo["genre"] = metadata.genre;
        }
        files.append(fileInfo);
      } else {
        db_->removeFile(filePath);
      }
    }
    response["status"] = "success";
    response["files"] = files;
    response["count"] = static_cast<int>(files.size());
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getArtists(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto artists = db_->getArtists();
    Json::Value artistsJson(Json::arrayValue);
    for (const auto &artist : artists) {
      artistsJson.append(artist);
    }
    response["status"] = "success";
    response["artists"] = artistsJson;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    bool clientSupportsGzip = false;
    auto acceptEncoding = req->getHeader("accept-encoding");
    if (acceptEncoding.find("gzip") != std::string::npos) {
      clientSupportsGzip = true;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    if (clientSupportsGzip && jsonStr.size() > 1024) {
      std::string compressed =
          drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
      if (!compressed.empty() && compressed.size() < jsonStr.size()) {
        resp->addHeader("Content-Encoding", "gzip");
        resp->setBody(std::move(compressed));
      } else {
        resp->setBody(std::move(jsonStr));
      }
    } else {
      resp->setBody(std::move(jsonStr));
    }
    callback(resp);
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }
}

void MusicController::getAlbums(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    std::string artistFilter = req->getParameter("artist");
    auto albums = db_->getAlbums(artistFilter);
    Json::Value albumsJson(Json::arrayValue);
    for (const auto &[album, artist, year] : albums) {
      Json::Value albumObj;
      albumObj["album"] = album;
      albumObj["artist"] = artist;
      albumObj["year"] = year;
      albumsJson.append(albumObj);
    }
    response["status"] = "success";
    response["albums"] = albumsJson;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    bool clientSupportsGzip = false;
    auto acceptEncoding = req->getHeader("accept-encoding");
    if (acceptEncoding.find("gzip") != std::string::npos) {
      clientSupportsGzip = true;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    if (clientSupportsGzip && jsonStr.size() > 1024) {
      std::string compressed =
          drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
      if (!compressed.empty() && compressed.size() < jsonStr.size()) {
        resp->addHeader("Content-Encoding", "gzip");
        resp->setBody(std::move(compressed));
      } else {
        resp->setBody(std::move(jsonStr));
      }
    } else {
      resp->setBody(std::move(jsonStr));
    }
    callback(resp);
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }
}

void MusicController::getAlbumsPaginated(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    std::string artistFilter = req->getParameter("artist");
    int page = req->getParameter("page").empty()
                   ? 1
                   : std::stoi(req->getParameter("page"));
    int pageSize = req->getParameter("pageSize").empty()
                       ? DEFAULT_PAGE_SIZE
                       : std::stoi(req->getParameter("pageSize"));
    if (pageSize > MAX_PAGE_SIZE)
      pageSize = MAX_PAGE_SIZE;
    if (page < 1)
      page = 1;
    int offset = (page - 1) * pageSize;
    auto allAlbums = db_->getAlbums(artistFilter);
    int totalCount = allAlbums.size();
    int totalPages = (totalCount + pageSize - 1) / pageSize;
    Json::Value albumsJson(Json::arrayValue);
    int start = offset;
    int end = std::min(offset + pageSize, totalCount);
    for (int i = start; i < end; ++i) {
      const auto &[album, artist, year] = allAlbums[i];
      Json::Value albumObj;
      albumObj["album"] = album;
      albumObj["artist"] = artist;
      albumObj["year"] = year;
      albumsJson.append(albumObj);
    }
    response["status"] = "success";
    response["albums"] = albumsJson;
    response["pagination"]["currentPage"] = page;
    response["pagination"]["pageSize"] = pageSize;
    response["pagination"]["totalCount"] = totalCount;
    response["pagination"]["totalPages"] = totalPages;
    response["pagination"]["hasNext"] = page < totalPages;
    response["pagination"]["hasPrev"] = page > 1;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    bool clientSupportsGzip = false;
    auto acceptEncoding = req->getHeader("accept-encoding");
    if (acceptEncoding.find("gzip") != std::string::npos) {
      clientSupportsGzip = true;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    if (clientSupportsGzip && jsonStr.size() > 1024) {
      std::string compressed =
          drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
      if (!compressed.empty() && compressed.size() < jsonStr.size()) {
        resp->addHeader("Content-Encoding", "gzip");
        resp->setBody(std::move(compressed));
      } else {
        resp->setBody(std::move(jsonStr));
      }
    } else {
      resp->setBody(std::move(jsonStr));
    }
    callback(resp);
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }
}

void MusicController::getAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &path) {
  std::string decodedPath = drogon::utils::urlDecode(path);
  auto albumArt = db_->getAlbumArt(decodedPath);
  if (albumArt.data.empty()) {
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  std::string mimeType = albumArt.mimeType;
  if (mimeType.empty()) {
    mimeType = AlbumArtExtractor::getMimeTypeFromData(albumArt.data);
  }
  if (mimeType == "image/jpeg") {
    resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
  } else if (mimeType == "image/png") {
    resp->setContentTypeCode(drogon::CT_IMAGE_PNG);
  } else if (mimeType == "image/gif") {
    resp->setContentTypeCode(drogon::CT_IMAGE_GIF);
  } else {
    resp->setContentTypeCode(drogon::CT_APPLICATION_OCTET_STREAM);
  }
  resp->setBody(std::string(albumArt.data.data(), albumArt.data.size()));
  callback(resp);
}

void MusicController::scan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    scanNewFiles();
    removeMissingFiles();
    response["status"] = "success";
    response["message"] = "Scan completed";
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::removeMissing(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    removeMissingFiles();
    response["status"] = "success";
    response["message"] = "Missing files removed";
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::scanNewFiles() {
  std::thread([this]() {
    auto dbFiles = db_->getAllFiles();
    std::unordered_set<std::string> existingFiles(dbFiles.begin(),
                                                  dbFiles.end());
    std::vector<fs::path> musicFiles;
    if (fs::exists(musicDir_)) {
      for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
        if (entry.is_regular_file()) {
          auto ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" ||
              ext == ".wav") {
            musicFiles.push_back(entry.path());
          }
        }
      }
    }
    for (const auto &filePath : musicFiles) {
      std::string pathStr = filePath.string();
      if (existingFiles.find(pathStr) == existingFiles.end()) {
        MusicMetadata metadata;
        if (extractMetadata(pathStr, metadata)) {
          if (db_->addFile(pathStr, metadata)) {
            std::cout << "Added new file: " << pathStr << '\n';
            std::vector<char> albumArt;
            if (extractAlbumArt(pathStr, albumArt)) {
              db_->saveAlbumArt(pathStr, albumArt);
            }
          }
        }
      }
    }
  });
}

void MusicController::removeMissingFiles() {
  auto allFiles = db_->getAllFiles();
  for (const auto &filePath : allFiles) {
    if (!fs::exists(filePath)) {
      db_->removeFile(filePath);
      std::cout << "Removed missing file: " << filePath << '\n';
    }
  }
}

std::string MusicController::fixTagLibString(const TagLib::String &str) {
  return str.to8Bit(true);
}

bool MusicController::extractAlbumArt(const std::string &filePath,
                                      std::vector<char> &albumArt) {
  try {
    std::string ext = filePath.substr(filePath.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".flac")
      return false;
    TagLib::FLAC::File flacFile(filePath.c_str());
    if (!flacFile.isOpen())
      return false;
    auto pictures = flacFile.pictureList();
    if (pictures.isEmpty())
      return false;
    TagLib::FLAC::Picture *bestPicture = nullptr;
    for (auto it = pictures.begin(); it != pictures.end(); ++it) {
      TagLib::FLAC::Picture *picture = *it;
      if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
        bestPicture = picture;
        break;
      }
      if (!bestPicture)
        bestPicture = picture;
    }
    if (!bestPicture)
      return false;
    TagLib::ByteVector data = bestPicture->data();
    albumArt.assign(data.data(), data.data() + data.size());
    return true;
  } catch (const std::exception &e) {
    std::cout << "Error extracting album art from " << filePath << ": "
              << e.what() << '\n';
  }
  return false;
}

void MusicController::getTracksByArtist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &artist) {
  Json::Value response;
  try {
    std::string decodedArtist = drogon::utils::urlDecode(artist);
    auto tracks = db_->getTracksByArtist(decodedArtist);
    Json::Value tracksJson(Json::arrayValue);
    for (const auto &track : tracks) {
      Json::Value trackObj;
      trackObj["path"] = track.filePath;
      trackObj["title"] = track.title;
      trackObj["artist"] = track.artist;
      trackObj["album"] = track.album;
      trackObj["duration"] = track.duration;
      trackObj["track"] = track.track;
      trackObj["year"] = track.year;
      trackObj["genre"] = track.genre;
      tracksJson.append(trackObj);
    }
    response["status"] = "success";
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracks.size());
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    bool clientSupportsGzip = false;
    auto acceptEncoding = req->getHeader("accept-encoding");
    if (acceptEncoding.find("gzip") != std::string::npos) {
      clientSupportsGzip = true;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    if (clientSupportsGzip && jsonStr.size() > 1024) {
      std::string compressed =
          drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
      if (!compressed.empty() && compressed.size() < jsonStr.size()) {
        resp->addHeader("Content-Encoding", "gzip");
        resp->setBody(std::move(compressed));
      } else {
        resp->setBody(std::move(jsonStr));
      }
    } else {
      resp->setBody(std::move(jsonStr));
    }
    callback(resp);
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }
}

void MusicController::getTracksByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  Json::Value response;
  try {
    std::string decodedAlbum = drogon::utils::urlDecode(album);
    std::string artistFilter = req->getParameter("artist");
    auto tracks = db_->getTracksByAlbum(decodedAlbum, artistFilter);
    Json::Value tracksJson(Json::arrayValue);
    for (const auto &track : tracks) {
      Json::Value trackObj;
      trackObj["path"] = track.filePath;
      trackObj["title"] = track.title.empty() ? "Unknown" : track.title;
      trackObj["artist"] = track.artist;
      trackObj["album"] = track.album;
      trackObj["duration"] = track.duration;
      trackObj["track"] = track.track;
      trackObj["year"] = track.year;
      trackObj["genre"] = track.genre;
      tracksJson.append(trackObj);
    }
    response["status"] = "success";
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracks.size());
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    bool clientSupportsGzip = false;
    auto acceptEncoding = req->getHeader("accept-encoding");
    if (acceptEncoding.find("gzip") != std::string::npos) {
      clientSupportsGzip = true;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    if (clientSupportsGzip && jsonStr.size() > 1024) {
      std::string compressed =
          drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
      if (!compressed.empty() && compressed.size() < jsonStr.size()) {
        resp->addHeader("Content-Encoding", "gzip");
        resp->setBody(std::move(compressed));
      } else {
        resp->setBody(std::move(jsonStr));
      }
    } else {
      resp->setBody(std::move(jsonStr));
    }
    callback(resp);
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
  }
}

void MusicController::getAlbumArtByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  std::string decodedAlbum = drogon::utils::urlDecode(album);
  std::string artistFilter = req->getParameter("artist");
  std::string filePath = db_->getFilePathByAlbum(decodedAlbum, artistFilter);
  if (filePath.empty()) {
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  auto albumArt = db_->getAlbumArt(filePath);
  if (albumArt.data.empty()) {
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  std::string mimeType = albumArt.mimeType;
  if (mimeType.empty()) {
    if (albumArt.data.size() >= 8) {
      if (albumArt.data[0] == 0xFF && albumArt.data[1] == 0xD8) {
        mimeType = "image/jpeg";
      } else if (albumArt.data[0] == 0x89 && albumArt.data[1] == 0x50 &&
                 albumArt.data[2] == 0x4E && albumArt.data[3] == 0x47) {
        mimeType = "image/png";
      } else if (albumArt.data[0] == 0x47 && albumArt.data[1] == 0x49 &&
                 albumArt.data[2] == 0x46) {
        mimeType = "image/gif";
      }
    }
  }
  if (mimeType == "image/jpeg") {
    resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
  } else if (mimeType == "image/png") {
    resp->setContentTypeCode(drogon::CT_IMAGE_PNG);
  } else if (mimeType == "image/gif") {
    resp->setContentTypeCode(drogon::CT_IMAGE_GIF);
  } else {
    resp->setContentTypeCode(drogon::CT_APPLICATION_OCTET_STREAM);
  }
  resp->setBody(std::string(albumArt.data.data(), albumArt.data.size()));
  callback(resp);
}

void MusicController::getFileMetadata(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  std::string filePath = req->getParameter("path");
  if (filePath.empty()) {
    response["status"] = "error";
    response["message"] = "Parameter 'path' is required";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    MusicMetadata dbMetadata;
    bool dbExists = db_->getMetadata(decodedPath, dbMetadata);
    MusicMetadata fileMetadata;
    bool fileRead = extractMetadata(decodedPath, fileMetadata);
    response["status"] = "success";
    Json::Value result;
    result["path"] = decodedPath;
    result["file_exists"] = fs::exists(decodedPath);
    Json::Value dbData;
    dbData["exists"] = dbExists;
    if (dbExists) {
      dbData["title"] = dbMetadata.title;
      dbData["artist"] = dbMetadata.artist;
      dbData["album"] = dbMetadata.album;
      dbData["track"] = dbMetadata.track;
      dbData["year"] = dbMetadata.year;
      dbData["genre"] = dbMetadata.genre;
      dbData["duration"] = dbMetadata.duration;
    }
    result["database"] = dbData;
    Json::Value fileData;
    fileData["readable"] = fileRead;
    if (fileRead) {
      fileData["title"] = fileMetadata.title;
      fileData["artist"] = fileMetadata.artist;
      fileData["album"] = fileMetadata.album;
      fileData["track"] = fileMetadata.track;
      fileData["year"] = fileMetadata.year;
      fileData["genre"] = fileMetadata.genre;
      fileData["duration"] = fileMetadata.duration;
    }
    result["file"] = fileData;
    Json::Value comparison;
    comparison["title_matches"] = (dbMetadata.title == fileMetadata.title);
    comparison["artist_matches"] = (dbMetadata.artist == fileMetadata.artist);
    comparison["album_matches"] = (dbMetadata.album == fileMetadata.album);
    comparison["track_matches"] = (dbMetadata.track == fileMetadata.track);
    result["comparison"] = comparison;
    Json::Value recommendations(Json::arrayValue);
    if (!fileMetadata.title.empty() && dbMetadata.title != fileMetadata.title) {
      recommendations.append(
          "Title in file differs from database - consider refreshing metadata");
    }
    if (fileMetadata.title.empty()) {
      recommendations.append(
          "File has no title tag - using filename as fallback");
      recommendations.append("Consider tagging files with proper metadata "
                             "using MusicBrainz Picard or similar");
    }
    if (!fileRead) {
      recommendations.append(
          "Cannot read metadata from file - check file format and permissions");
    }
    result["recommendations"] = recommendations;
    response["data"] = result;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getDatabaseStats(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto allFiles = db_->getAllFiles();
    int totalFiles = allFiles.size();
    int filesWithTags = 0;
    int filesWithoutTitle = 0;
    int filesWithArtist = 0;
    int filesWithAlbum = 0;
    std::unordered_set<std::string> uniqueArtists;
    std::unordered_set<std::string> uniqueAlbums;
    for (const auto &filePath : allFiles) {
      MusicMetadata metadata;
      if (db_->getMetadata(filePath, metadata)) {
        if (!metadata.title.empty() && metadata.title != "Unknown") {
          filesWithTags++;
        } else {
          filesWithoutTitle++;
        }
        if (!metadata.artist.empty() && metadata.artist != "Unknown") {
          filesWithArtist++;
          uniqueArtists.insert(metadata.artist);
        }
        if (!metadata.album.empty() && metadata.album != "Unknown") {
          filesWithAlbum++;
          uniqueAlbums.insert(metadata.album);
        }
      }
    }
    response["status"] = "success";
    Json::Value dataObj;
    dataObj["total_files"] = totalFiles;
    dataObj["files_with_tags"] = filesWithTags;
    dataObj["files_without_title"] = filesWithoutTitle;
    dataObj["files_with_artist"] = filesWithArtist;
    dataObj["files_with_album"] = filesWithAlbum;
    dataObj["unique_artists"] = static_cast<int>(uniqueArtists.size());
    dataObj["unique_albums"] = static_cast<int>(uniqueAlbums.size());
    dataObj["tag_coverage_percent"] =
        totalFiles > 0 ? (filesWithTags * 100 / totalFiles) : 0;
    response["data"] = dataObj;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::forceRescan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  LOG_INFO << "forceRescan called";
  {
    std::lock_guard<std::mutex> lock(rescanStatusMutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - rescanStatus_.lastRescanTime)
                       .count();
    if (rescanStatus_.inProgress || elapsed < 5) {
      LOG_WARN << "Rescan rejected - inProgress=" << rescanStatus_.inProgress
               << " elapsed=" << elapsed;
      Json::Value response;
      response["status"] = "error";
      response["message"] = "Rescan already in progress or too frequent";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      callback(resp);
      return;
    }
    rescanStatus_ = RescanStatus();
    rescanStatus_.inProgress = true;
    rescanStatus_.lastRescanTime = now;
    LOG_INFO << "Rescan started";
  }
  Json::Value response;
  response["status"] = "success";
  response["message"] = "Force rescan started in background";
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
  std::thread([this]() {
    LOG_INFO << "Rescan thread started";
    try {
      auto oldAlbums = db_->getAlbums();
      {
        std::lock_guard<std::mutex> lock(rescanStatusMutex_);
        rescanStatus_.oldAlbumsCount = oldAlbums.size();
      }
      std::vector<fs::path> musicFiles;
      if (fs::exists(musicDir_)) {
        for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
          if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" ||
                ext == ".wav") {
              musicFiles.push_back(entry.path());
            }
          }
        }
      }
      {
        std::lock_guard<std::mutex> lock(rescanStatusMutex_);
        rescanStatus_.totalFiles = musicFiles.size();
      }
      auto allFiles = db_->getAllFiles();
      for (const auto &filePath : allFiles) {
        db_->removeFile(filePath);
      }
      for (size_t i = 0; i < musicFiles.size(); i++) {
        std::string pathStr = musicFiles[i].string();
        MusicMetadata metadata;
        if (extractMetadata(pathStr, metadata)) {
          if (db_->addFile(pathStr, metadata)) {
            {
              std::lock_guard<std::mutex> lock(rescanStatusMutex_);
              rescanStatus_.addedFiles++;
            }
            std::vector<char> albumArt;
            if (extractAlbumArt(pathStr, albumArt)) {
              db_->saveAlbumArt(pathStr, albumArt);
            }
          } else {
            std::lock_guard<std::mutex> lock(rescanStatusMutex_);
            rescanStatus_.errorCount++;
          }
        } else {
          std::lock_guard<std::mutex> lock(rescanStatusMutex_);
          rescanStatus_.errorCount++;
        }
        {
          std::lock_guard<std::mutex> lock(rescanStatusMutex_);
          rescanStatus_.processedFiles = i + 1;
        }
      }
      auto newAlbums = db_->getAlbums();
      {
        std::lock_guard<std::mutex> lock(rescanStatusMutex_);
        rescanStatus_.newAlbumsCount = newAlbums.size();
        rescanStatus_.inProgress = false;
        LOG_INFO << "Rescan completed. Added files: "
                 << rescanStatus_.addedFiles
                 << ", New albums: " << rescanStatus_.newAlbumsCount;
      }
    } catch (const std::exception &e) {
      LOG_ERROR << "Rescan failed: " << e.what();
      std::lock_guard<std::mutex> lock(rescanStatusMutex_);
      rescanStatus_.inProgress = false;
    }
  }).detach();
}

bool MusicController::extractMetadataWithTagEditor(const std::string &filePath,
                                                   MusicMetadata &metadata) {
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".flac") {
    return false;
  }
  try {
    TagEditor editor(filePath);
    if (!editor.load()) {
      LOG_WARN << "Failed to load file with TagEditor: " << filePath;
      return false;
    }
    metadata.title = editor.getTitle();
    metadata.artist = editor.getArtist();
    metadata.album = editor.getAlbum();
    metadata.genre = editor.getGenre();
    metadata.track = editor.getTrackNumber();
    std::string date = editor.getDate();
    if (!date.empty() && date.length() >= 4) {
      try {
        metadata.year = std::stoi(date.substr(0, 4));
      } catch (...) {
        metadata.year = 0;
      }
    } else {
      metadata.year = 0;
    }
    metadata.duration = 0;
    return true;
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting metadata with TagEditor from " << filePath
              << ": " << e.what();
    return false;
  }
}

bool MusicController::updateFileTagsInternal(const std::string &filePath,
                                             const MusicMetadata &metadata) {
  std::string ext = fs::path(filePath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".flac") {
    return false;
  }
  try {
    TagEditor editor(filePath);
    if (!editor.load()) {
      return false;
    }
    if (!metadata.title.empty()) {
      editor.setTitle(metadata.title);
    }
    if (!metadata.artist.empty()) {
      editor.setArtist(metadata.artist);
    }
    if (!metadata.album.empty()) {
      editor.setAlbum(metadata.album);
    }
    if (!metadata.genre.empty()) {
      editor.setGenre(metadata.genre);
    }
    if (metadata.track > 0) {
      editor.setTrackNumber(metadata.track);
    }
    if (metadata.year > 0) {
      editor.setDate(std::to_string(metadata.year));
    }
    if (!editor.save()) {
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

void MusicController::deleteAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  auto json = req->getJsonObject();
  if (!json || !json->isMember("album") || !json->isMember("artist")) {
    response["status"] = "error";
    response["message"] = "Missing album or artist parameter";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string albumName = (*json)["album"].asString();
  std::string artistName = (*json)["artist"].asString();
  std::string decodedAlbum = drogon::utils::urlDecode(albumName);
  std::string decodedArtist = drogon::utils::urlDecode(artistName);
  try {
    auto tracks = db_->getTracksByAlbum(decodedAlbum, decodedArtist);
    if (tracks.empty()) {
      response["status"] = "error";
      response["message"] = "Album not found";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    int deletedCount = 0;
    int errorCount = 0;
    for (const auto &track : tracks) {
      if (fs::exists(track.filePath)) {
        std::string trashCmd =
            "kioclient5 move \"" + track.filePath + "\" trash:/ 2>/dev/null";
        int result = system(trashCmd.c_str());
        if (result == 0) {
          deletedCount++;
        } else {
          errorCount++;
        }
      }
      db_->removeFile(track.filePath);
    }
    response["status"] = "success";
    response["message"] = "Album deleted";
    response["deleted_files"] = deletedCount;
    response["error_count"] = errorCount;
    response["album"] = decodedAlbum;
    response["artist"] = decodedArtist;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getRescanStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  std::lock_guard<std::mutex> lock(rescanStatusMutex_);
  response["status"] = "success";
  response["in_progress"] = rescanStatus_.inProgress;
  response["total_files"] = rescanStatus_.totalFiles;
  response["processed_files"] = rescanStatus_.processedFiles;
  response["added_files"] = rescanStatus_.addedFiles;
  response["error_count"] = rescanStatus_.errorCount;
  response["old_albums_count"] = rescanStatus_.oldAlbumsCount;
  response["new_albums_count"] = rescanStatus_.newAlbumsCount;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}
