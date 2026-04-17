#include "controllers/MusicController.h"
#include "services/AlbumArtExtractor.h"
#include "tagger/TagEditor.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <iostream>
#include <json/json.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <unordered_set>

namespace fs = std::filesystem;

std::shared_ptr<PlayerService> MusicController::playerService_ = nullptr;

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

void MusicController::setPlayerService(std::shared_ptr<PlayerService> service) {
  playerService_ = service;
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
            LOG_INFO << "Added new file: " << pathStr;
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
      LOG_INFO << "Removed missing file: " << filePath;
    }
  }
}

#include <regex>

bool MusicController::extractMetadata(const std::string &filePath,
                                      MusicMetadata &metadata) {
  if (!fs::exists(filePath))
    return false;
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      TagLib::String title = tag->title();
      TagLib::String artist = tag->artist();
      TagLib::String album = tag->album();
      TagLib::String genre = tag->genre();
      metadata.title = title.to8Bit(true);
      metadata.artist = artist.to8Bit(true);
      metadata.album = album.to8Bit(true);
      metadata.genre = genre.to8Bit(true);
      if (f.audioProperties())
        metadata.duration = f.audioProperties()->lengthInSeconds();
      else
        metadata.duration = 0;
      metadata.track = tag->track();
      metadata.year = tag->year();
      return true;
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting metadata: " << e.what();
  }
  std::string filename = fs::path(filePath).stem().string();
  static const std::regex trackPrefix(R"(^\s*\d{1,2}[\.\-\s]+\s*)");
  filename = std::regex_replace(filename, trackPrefix, "");
  if (filename.find_last_of('.') != std::string::npos) {
    filename = filename.substr(0, filename.find_last_of('.'));
  }
  metadata.title = filename;
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
    if (playerService_) {
      playerService_->setPlaylist(tracks);
      playerService_->play();
      response["status"] = "success";
      response["message"] = "Musium launched via PlayerService";
      response["tracks_count"] = static_cast<int>(tracks.size());
    } else {
      response["status"] = "error";
      response["message"] = "PlayerService not available";
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
  Json::Value response;
  try {
    LOG_INFO << "Force rescan started - full database refresh";
    auto allFiles = db_->getAllFiles();
    int removedCount = 0;
    for (const auto &filePath : allFiles) {
      if (db_->removeFile(filePath)) {
        removedCount++;
      }
    }
    LOG_INFO << "Removed " << removedCount << " old entries from database";
    std::vector<fs::path> musicFiles;
    if (fs::exists(musicDir_)) {
      for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
        if (entry.is_regular_file()) {
          auto ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" ||
              ext == ".wav" || ext == ".ogg" || ext == ".opus") {
            musicFiles.push_back(entry.path());
          }
        }
      }
    }
    int addedCount = 0;
    int errorCount = 0;
    for (const auto &filePath : musicFiles) {
      std::string pathStr = filePath.string();
      MusicMetadata metadata;
      if (extractMetadata(pathStr, metadata)) {
        if (db_->addFile(pathStr, metadata)) {
          addedCount++;
          std::vector<char> albumArt;
          if (extractAlbumArt(pathStr, albumArt)) {
            db_->saveAlbumArt(pathStr, albumArt);
          }
        } else {
          errorCount++;
        }
      } else {
        errorCount++;
      }
    }
    response["status"] = "success";
    response["message"] = "Force rescan completed";
    response["removed_files"] = removedCount;
    response["added_files"] = addedCount;
    response["error_count"] = errorCount;
    response["total_files"] = addedCount;
    LOG_INFO << "Force rescan finished. Added: " << addedCount
             << ", Errors: " << errorCount;
  } catch (const std::exception &e) {
    LOG_ERROR << "Force rescan error: " << e.what();
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
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
  std::cout << "[DEBUG] updateFileTagsInternal called for: " << filePath
            << std::endl;
  std::string ext = fs::path(filePath).extension().string();
  std::cout << "[DEBUG] File extension: '" << ext << "'" << std::endl;
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  std::cout << "[DEBUG] Normalized extension: '" << ext << "'" << std::endl;
  if (ext != ".flac") {
    std::cout << "[DEBUG] Not a FLAC file, returning false" << std::endl;
    return false;
  }
  try {
    TagEditor editor(filePath);
    if (!editor.load()) {
      std::cout << "[DEBUG] Failed to load file with TagEditor" << std::endl;
      return false;
    }
    std::cout << "[DEBUG] TagEditor loaded successfully" << std::endl;
    if (!metadata.title.empty()) {
      editor.setTitle(metadata.title);
      std::cout << "[DEBUG] Set title to: " << metadata.title << std::endl;
    }
    if (!metadata.artist.empty()) {
      editor.setArtist(metadata.artist);
      std::cout << "[DEBUG] Set artist to: " << metadata.artist << std::endl;
    }
    if (!metadata.album.empty()) {
      editor.setAlbum(metadata.album);
      std::cout << "[DEBUG] Set album to: " << metadata.album << std::endl;
    }
    if (!metadata.genre.empty()) {
      editor.setGenre(metadata.genre);
    }
    if (metadata.track > 0) {
      editor.setTrackNumber(metadata.track);
      std::cout << "[DEBUG] Set track to: " << metadata.track << std::endl;
    }
    if (metadata.year > 0) {
      editor.setDate(std::to_string(metadata.year));
      std::cout << "[DEBUG] Set year to: " << metadata.year << std::endl;
    }
    if (!editor.save()) {
      std::cout << "[DEBUG] Failed to save tags" << std::endl;
      return false;
    }
    std::cout << "[DEBUG] Tags saved successfully" << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cout << "[DEBUG] Exception: " << e.what() << std::endl;
    return false;
  }
}

void MusicController::updateFileTags(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  auto json = req->getJsonObject();
  std::cout << "[DEBUG] updateFileTags called" << std::endl;
  if (!json) {
    std::cout << "[DEBUG] No JSON body" << std::endl;
    response["status"] = "error";
    response["message"] = "Invalid JSON body";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  if (!json->isMember("path")) {
    std::cout << "[DEBUG] No path parameter" << std::endl;
    response["status"] = "error";
    response["message"] = "Parameter 'path' is required";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  std::cout << "[DEBUG] Path: " << decodedPath << std::endl;
  try {
    if (!fs::exists(decodedPath)) {
      std::cout << "[DEBUG] File does not exist" << std::endl;
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
      std::cout << "[DEBUG] Title from JSON: " << newMetadata.title
                << std::endl;
    }
    if (json->isMember("artist")) {
      newMetadata.artist = (*json)["artist"].asString();
      std::cout << "[DEBUG] Artist from JSON: " << newMetadata.artist
                << std::endl;
    }
    if (json->isMember("album")) {
      newMetadata.album = (*json)["album"].asString();
      std::cout << "[DEBUG] Album from JSON: " << newMetadata.album
                << std::endl;
    }
    if (json->isMember("genre")) {
      newMetadata.genre = (*json)["genre"].asString();
    }
    if (json->isMember("track")) {
      newMetadata.track = (*json)["track"].asInt();
      std::cout << "[DEBUG] Track from JSON: " << newMetadata.track
                << std::endl;
    }
    if (json->isMember("year")) {
      newMetadata.year = (*json)["year"].asInt();
      std::cout << "[DEBUG] Year from JSON: " << newMetadata.year << std::endl;
    }
    bool tagsUpdated = updateFileTagsInternal(decodedPath, newMetadata);
    if (!tagsUpdated) {
      std::cout << "[DEBUG] updateFileTagsInternal returned false" << std::endl;
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
    std::cout << "[DEBUG] Exception: " << e.what() << std::endl;
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
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
