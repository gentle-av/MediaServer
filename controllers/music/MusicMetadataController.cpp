#include "controllers/music/MusicMetadataController.h"
#include "services/music/MetadataExtractor.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicDatabase *MusicMetadataController::db_ = nullptr;
MetadataCache *MusicMetadataController::cache_ = nullptr;

MusicMetadataController::MusicMetadataController() {}

void MusicMetadataController::init(std::unique_ptr<MusicDatabase> &db,
                                   std::unique_ptr<MetadataCache> &cache) {
  db_ = db.get();
  cache_ = cache.get();
}

void MusicMetadataController::getFileMetadata(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string filePath = req->getParameter("path");
  if (filePath.empty()) {
    ResponseBuilder::sendError(callback, "Parameter 'path' is required");
    return;
  }
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    MusicMetadata dbMetadata;
    bool dbExists = db_->getMetadata(decodedPath, dbMetadata);
    MusicMetadata fileMetadata;
    bool fileRead =
        MetadataExtractor::extractMetadata(decodedPath, fileMetadata);
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
    }
    result["recommendations"] = recommendations;
    ResponseBuilder::sendSuccess(callback, result);
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicMetadataController::refreshFileMetadata(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    ResponseBuilder::sendError(callback,
                               "Parameter 'path' is required in JSON body");
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    if (!fs::exists(decodedPath)) {
      ResponseBuilder::sendError(callback, "File does not exist",
                                 drogon::k404NotFound);
      return;
    }
    MusicMetadata metadata;
    if (MetadataExtractor::extractMetadata(decodedPath, metadata)) {
      if (db_->addFile(decodedPath, metadata)) {
        std::vector<char> albumArt;
        if (MetadataExtractor::extractAlbumArt(decodedPath, albumArt)) {
          db_->saveAlbumArt(decodedPath, albumArt);
        }
        cache_->erase(decodedPath);
        cache_->put(decodedPath, metadata);
        Json::Value data;
        data["path"] = decodedPath;
        data["title"] = metadata.title;
        data["artist"] = metadata.artist;
        data["album"] = metadata.album;
        data["track"] = metadata.track;
        ResponseBuilder::sendSuccess(callback, data);
      } else {
        ResponseBuilder::sendError(callback,
                                   "Failed to save metadata to database");
      }
    } else {
      ResponseBuilder::sendError(callback,
                                 "Failed to extract metadata from file");
    }
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicMetadataController::updateFileTags(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    ResponseBuilder::sendError(callback, "Parameter 'path' is required");
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  try {
    if (!fs::exists(decodedPath)) {
      ResponseBuilder::sendError(callback, "File does not exist",
                                 drogon::k404NotFound);
      return;
    }
    MusicMetadata newMetadata;
    if (json->isMember("title"))
      newMetadata.title = (*json)["title"].asString();
    if (json->isMember("artist"))
      newMetadata.artist = (*json)["artist"].asString();
    if (json->isMember("album"))
      newMetadata.album = (*json)["album"].asString();
    if (json->isMember("genre"))
      newMetadata.genre = (*json)["genre"].asString();
    if (json->isMember("track"))
      newMetadata.track = (*json)["track"].asInt();
    if (json->isMember("year"))
      newMetadata.year = (*json)["year"].asInt();
    if (!MetadataExtractor::updateFileTags(decodedPath, newMetadata)) {
      ResponseBuilder::sendError(
          callback, "Failed to update tags. Only FLAC files are supported.",
          drogon::k500InternalServerError);
      return;
    }
    MusicMetadata updatedMetadata;
    if (MetadataExtractor::extractMetadata(decodedPath, updatedMetadata)) {
      db_->addFile(decodedPath, updatedMetadata);
      cache_->erase(decodedPath);
      cache_->put(decodedPath, updatedMetadata);
      if (json->isMember("album")) {
        std::vector<char> albumArt;
        if (MetadataExtractor::extractAlbumArt(decodedPath, albumArt)) {
          db_->saveAlbumArt(decodedPath, albumArt);
        }
      }
    }
    Json::Value data;
    data["path"] = decodedPath;
    data["title"] = newMetadata.title;
    data["artist"] = newMetadata.artist;
    data["album"] = newMetadata.album;
    data["track"] = newMetadata.track;
    data["year"] = newMetadata.year;
    data["genre"] = newMetadata.genre;
    ResponseBuilder::sendSuccess(callback, data);
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicMetadataController::getDatabaseStats(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    auto allFiles = db_->getAllFiles();
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
    Json::Value data;
    data["total_files"] = static_cast<int>(allFiles.size());
    data["files_with_tags"] = filesWithTags;
    data["files_without_title"] = filesWithoutTitle;
    data["files_with_artist"] = filesWithArtist;
    data["files_with_album"] = filesWithAlbum;
    data["unique_artists"] = static_cast<int>(uniqueArtists.size());
    data["unique_albums"] = static_cast<int>(uniqueAlbums.size());
    data["tag_coverage_percent"] =
        allFiles.empty() ? 0 : (filesWithTags * 100 / allFiles.size());
    ResponseBuilder::sendSuccess(callback, data);
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}
