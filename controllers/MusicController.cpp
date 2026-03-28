#include "controllers/MusicController.h"
#include "services/AlbumArtExtractor.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <json/json.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <unordered_set>

namespace fs = std::filesystem;

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
  scanNewFiles();
  removeMissingFiles();
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
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
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
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
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
  auto dbFiles = db_->getAllFiles();
  std::unordered_set<std::string> existingFiles(dbFiles.begin(), dbFiles.end());
  std::vector<fs::path> musicFiles;
  if (fs::exists(musicDir_)) {
    for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
      if (entry.is_regular_file()) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" || ext == ".wav") {
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
          LOG_INFO << "Added new file: " << pathStr;
          std::vector<char> albumArt;
          if (extractAlbumArt(pathStr, albumArt)) {
            db_->saveAlbumArt(pathStr, albumArt);
          }
        }
      }
    }
  }
}

void MusicController::removeMissingFiles() {
  auto allFiles = db_->getAllFiles();
  for (const auto &filePath : allFiles) {
    if (!fs::exists(filePath)) {
      db_->removeFile(filePath);
      LOG_INFO << "Removed missing file: " << filePath;
    }
  }
}

bool MusicController::extractMetadata(const std::string &filePath,
                                      MusicMetadata &metadata) {
  if (!fs::exists(filePath))
    return false;
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      metadata.title = tag->title().to8Bit(true);
      metadata.artist = tag->artist().to8Bit(true);
      metadata.album = tag->album().to8Bit(true);
      if (f.audioProperties()) {
        metadata.duration = f.audioProperties()->lengthInSeconds();
      } else {
        metadata.duration = 0;
      }
      metadata.track = tag->track();
      metadata.year = tag->year();
      metadata.genre = tag->genre().to8Bit(true);
      return true;
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting metadata: " << e.what();
  }
  metadata.title = fs::path(filePath).stem().string();
  metadata.artist = "Unknown";
  metadata.album = "Unknown";
  return true;
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
    LOG_ERROR << "Error extracting album art from " << filePath << ": "
              << e.what();
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
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
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
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getAlbumArtByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  std::string decodedAlbum = drogon::utils::urlDecode(album);
  std::string artistFilter = req->getParameter("artist");
  LOG_INFO << "Looking for album art: " << decodedAlbum
           << " artist: " << artistFilter;
  std::string filePath = db_->getFilePathByAlbum(decodedAlbum, artistFilter);
  if (filePath.empty()) {
    LOG_INFO << "No file path found for album: " << decodedAlbum;
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  LOG_INFO << "Found file path: " << filePath;
  auto albumArt = db_->getAlbumArt(filePath);
  if (albumArt.data.empty()) {
    LOG_INFO << "No album art data for: " << filePath;
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  LOG_INFO << "Album art size: " << albumArt.data.size();
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
    std::string musiumPath = "/usr/local/bin/Musium";
    if (!std::filesystem::exists(musiumPath)) {
      musiumPath = "./Musium";
    }
    if (!std::filesystem::exists(musiumPath)) {
      musiumPath = "/opt/musium/Musium";
    }
    std::string cmd = musiumPath;
    for (const auto &track : tracks) {
      cmd += " \"" + track + "\"";
    }
    cmd += " --port 8084 --daemon < /dev/null > /tmp/musium.log 2>&1 & disown";
    LOG_INFO << "Executing: " << cmd;
    int result = std::system(cmd.c_str());
    if (result == 0) {
      response["status"] = "success";
      response["message"] = "Musium launched";
      response["tracks_count"] = static_cast<int>(tracks.size());
    } else {
      response["status"] = "error";
      response["message"] = "Failed to launch Musium";
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
