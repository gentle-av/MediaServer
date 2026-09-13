#include "controllers/music/MusicMetadataController.h"
#include "services/music/MetadataExtractor.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicMetadataController::MusicMetadataController(
    App &app, std::shared_ptr<MusicDatabase> db,
    std::shared_ptr<MetadataCache> cache)
    : RestController<App>(app), db(db), cache(cache) {}

void MusicMetadataController::register_all_routes() {
  this->app_.get("/api/music/file-metadata",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetFileMetadata(req);
                 });
  this->app_.post("/api/music/refresh-metadata",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleRefreshFileMetadata(req);
                  });
  this->app_.post("/api/music/update-tags",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleUpdateFileTags(req);
                  });
  this->app_.get("/api/music/stats",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetDatabaseStats(req);
                 });
}

StringHttpResponse
MusicMetadataController::handleGetFileMetadata(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string filePath = this->getQueryParam(req, "path");
  if (filePath.empty()) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Parameter 'path' is required").dump());
    return res;
  }
  std::string decodedPath = filePath;
  try {
    MusicMetadata dbMetadata;
    bool dbExists = db->getMetadata(decodedPath, dbMetadata);
    MusicMetadata fileMetadata;
    bool fileRead =
        MetadataExtractor::extractMetadata(decodedPath, fileMetadata);
    nlohmann::json result;
    result["path"] = decodedPath;
    result["file_exists"] = fs::exists(decodedPath);
    nlohmann::json dbData;
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
    nlohmann::json fileData;
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
    nlohmann::json comparison;
    comparison["title_matches"] = (dbMetadata.title == fileMetadata.title);
    comparison["artist_matches"] = (dbMetadata.artist == fileMetadata.artist);
    comparison["album_matches"] = (dbMetadata.album == fileMetadata.album);
    comparison["track_matches"] = (dbMetadata.track == fileMetadata.track);
    result["comparison"] = comparison;
    nlohmann::json recommendations = nlohmann::json::array();
    if (!fileMetadata.title.empty() && dbMetadata.title != fileMetadata.title) {
      recommendations.push_back(
          "Title in file differs from database - consider refreshing metadata");
    }
    if (fileMetadata.title.empty()) {
      recommendations.push_back(
          "File has no title tag - using filename as fallback");
    }
    result["recommendations"] = recommendations;
    result["success"] = true;
    res.setJsonContent(result.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse MusicMetadataController::handleRefreshFileMetadata(
    const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(req.getBody());
  } catch (...) {
    res.setStatus(400);
    res.setJsonContent(this->error_response(400, "Invalid JSON body").dump());
    return res;
  }
  if (!json.contains("path")) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Parameter 'path' is required in JSON body")
            .dump());
    return res;
  }
  std::string decodedPath = json["path"].get<std::string>();
  try {
    if (!fs::exists(decodedPath)) {
      res.setStatus(404);
      res.setJsonContent(
          this->error_response(404, "File does not exist").dump());
      return res;
    }
    MusicMetadata metadata;
    if (MetadataExtractor::extractMetadata(decodedPath, metadata)) {
      if (db->addFile(decodedPath, metadata)) {
        std::vector<char> albumArt;
        if (MetadataExtractor::extractAlbumArt(decodedPath, albumArt)) {
          db->saveAlbumArt(decodedPath, albumArt);
        }
        cache->erase(decodedPath);
        cache->put(decodedPath, metadata);
        nlohmann::json data = this->success_response("Metadata refreshed");
        data["path"] = decodedPath;
        data["title"] = metadata.title;
        data["artist"] = metadata.artist;
        data["album"] = metadata.album;
        data["track"] = metadata.track;
        res.setJsonContent(data.dump());
        res.setStatus(200);
      } else {
        res.setStatus(500);
        res.setJsonContent(
            this->error_response(500, "Failed to save metadata to database")
                .dump());
      }
    } else {
      res.setStatus(500);
      res.setJsonContent(
          this->error_response(500, "Failed to extract metadata from file")
              .dump());
    }
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
MusicMetadataController::handleUpdateFileTags(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(req.getBody());
  } catch (...) {
    res.setStatus(400);
    res.setJsonContent(this->error_response(400, "Invalid JSON body").dump());
    return res;
  }
  if (!json.contains("path")) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Parameter 'path' is required").dump());
    return res;
  }
  std::string decodedPath = json["path"].get<std::string>();
  try {
    if (!fs::exists(decodedPath)) {
      res.setStatus(404);
      res.setJsonContent(
          this->error_response(404, "File does not exist").dump());
      return res;
    }
    MusicMetadata newMetadata;
    if (json.contains("title"))
      newMetadata.title = json["title"].get<std::string>();
    if (json.contains("artist"))
      newMetadata.artist = json["artist"].get<std::string>();
    if (json.contains("album"))
      newMetadata.album = json["album"].get<std::string>();
    if (json.contains("genre"))
      newMetadata.genre = json["genre"].get<std::string>();
    if (json.contains("track"))
      newMetadata.track = json["track"].get<int>();
    if (json.contains("year"))
      newMetadata.year = json["year"].get<int>();
    if (!MetadataExtractor::updateFileTags(decodedPath, newMetadata)) {
      res.setStatus(500);
      res.setJsonContent(
          this->error_response(
                  500, "Failed to update tags. Only FLAC files are supported.")
              .dump());
      return res;
    }
    MusicMetadata updatedMetadata;
    if (MetadataExtractor::extractMetadata(decodedPath, updatedMetadata)) {
      db->addFile(decodedPath, updatedMetadata);
      cache->erase(decodedPath);
      cache->put(decodedPath, updatedMetadata);
      if (json.contains("album")) {
        std::vector<char> albumArt;
        if (MetadataExtractor::extractAlbumArt(decodedPath, albumArt)) {
          db->saveAlbumArt(decodedPath, albumArt);
        }
      }
    }
    nlohmann::json data = this->success_response("Tags updated successfully");
    data["path"] = decodedPath;
    data["title"] = newMetadata.title;
    data["artist"] = newMetadata.artist;
    data["album"] = newMetadata.album;
    data["track"] = newMetadata.track;
    data["year"] = newMetadata.year;
    data["genre"] = newMetadata.genre;
    res.setJsonContent(data.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
MusicMetadataController::handleGetDatabaseStats(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto allFiles = db->getAllFilePaths();
    int filesWithTags = 0;
    int filesWithoutTitle = 0;
    int filesWithArtist = 0;
    int filesWithAlbum = 0;
    std::unordered_set<std::string> uniqueArtists;
    std::unordered_set<std::string> uniqueAlbums;
    for (const auto &filePath : allFiles) {
      MusicMetadata metadata;
      if (db->getMetadata(filePath, metadata)) {
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
    nlohmann::json data = this->success_response("Stats retrieved");
    data["total_files"] = static_cast<int>(allFiles.size());
    data["files_with_tags"] = filesWithTags;
    data["files_without_title"] = filesWithoutTitle;
    data["files_with_artist"] = filesWithArtist;
    data["files_with_album"] = filesWithAlbum;
    data["unique_artists"] = static_cast<int>(uniqueArtists.size());
    data["unique_albums"] = static_cast<int>(uniqueAlbums.size());
    data["tag_coverage_percent"] =
        allFiles.empty() ? 0 : (filesWithTags * 100 / allFiles.size());
    res.setJsonContent(data.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

std::string
MusicMetadataController::getQueryParam(const StringHttpRequest &req,
                                       const std::string &key,
                                       const std::string &defaultValue) {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}
