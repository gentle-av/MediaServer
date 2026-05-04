#include "controllers/MusicController.h"
#include "services/music/MetadataExtractor.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>

namespace fs = std::filesystem;

std::shared_ptr<PlayerController> MusicController::playerController_ = nullptr;

MusicController::MusicController() {
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  fs::create_directories(fs::path(dbPath).parent_path());

  db_ = std::make_unique<MusicDatabase>(dbPath);
  db_->init();

  cache_ = std::make_unique<MetadataCache>();
  scanner_ = std::make_unique<MusicScanner>(*db_, *cache_, "/mnt/media/music");
  albumArtService_ = std::make_unique<AlbumArtService>(*db_);
  playlistManager_ = std::make_unique<PlaylistManager>(playerController_, *db_);
}

void MusicController::setPlayerController(
    std::shared_ptr<PlayerController> controller) {
  playerController_ = controller;
}

void MusicController::getTracksByArtist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &artist) {
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

    Json::Value response;
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracks.size());
    callback(ResponseBuilder::compressedJson(response, req));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getTracksByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
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

    Json::Value response;
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracks.size());
    callback(ResponseBuilder::compressedJson(response, req));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::listFiles(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
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

    Json::Value response;
    response["files"] = files;
    response["count"] = static_cast<int>(files.size());
    callback(ResponseBuilder::success(response));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getArtists(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    auto artists = db_->getArtists();
    Json::Value artistsJson(Json::arrayValue);
    for (const auto &artist : artists) {
      artistsJson.append(artist);
    }

    Json::Value response;
    response["artists"] = artistsJson;
    callback(ResponseBuilder::compressedJson(response, req));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getAlbums(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
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

    Json::Value response;
    response["albums"] = albumsJson;
    callback(ResponseBuilder::compressedJson(response, req));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getAlbumsPaginated(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    std::string artistFilter = req->getParameter("artist");
    int page = req->getParameter("page").empty()
                   ? 1
                   : std::stoi(req->getParameter("page"));
    int pageSize = req->getParameter("pageSize").empty()
                       ? 20
                       : std::stoi(req->getParameter("pageSize"));

    if (pageSize > 50)
      pageSize = 50;
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

    Json::Value response;
    response["albums"] = albumsJson;
    response["pagination"]["currentPage"] = page;
    response["pagination"]["pageSize"] = pageSize;
    response["pagination"]["totalCount"] = totalCount;
    response["pagination"]["totalPages"] = totalPages;
    response["pagination"]["hasNext"] = page < totalPages;
    response["pagination"]["hasPrev"] = page > 1;

    callback(ResponseBuilder::compressedJson(response, req));
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string filePath = req->getParameter("path");
  if (filePath.empty()) {
    callback(drogon::HttpResponse::newNotFoundResponse());
    return;
  }

  std::string decodedPath = drogon::utils::urlDecode(filePath);
  auto albumArt = albumArtService_->getAlbumArt(decodedPath);
  callback(albumArtService_->createImageResponse(albumArt));
}

void MusicController::getAlbumArtByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  std::string decodedAlbum = drogon::utils::urlDecode(album);
  std::string artistFilter = req->getParameter("artist");

  auto albumArt =
      albumArtService_->getAlbumArtByAlbum(decodedAlbum, artistFilter);
  callback(albumArtService_->createImageResponse(albumArt));
}

void MusicController::scan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    scanner_->scanNewFiles();
    ResponseBuilder::sendSuccess(callback, Json::Value());
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::removeMissing(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    scanner_->removeMissingFiles();
    ResponseBuilder::sendSuccess(callback, Json::Value());
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::openMusium(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("tracks") || !(*json)["tracks"].isArray()) {
    ResponseBuilder::sendError(callback, "Missing tracks array parameter");
    return;
  }

  std::vector<std::string> tracks;
  for (const auto &track : (*json)["tracks"]) {
    if (track.isString())
      tracks.push_back(track.asString());
  }

  if (tracks.empty()) {
    ResponseBuilder::sendError(callback, "No tracks provided");
    return;
  }

  if (playlistManager_->openMusium(tracks)) {
    Json::Value data;
    data["tracks_count"] = static_cast<int>(tracks.size());
    ResponseBuilder::sendSuccess(callback, data);
  } else {
    ResponseBuilder::sendError(callback, "Failed to open Musium",
                               drogon::k500InternalServerError);
  }
}

void MusicController::getFileMetadata(
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

    // Database metadata
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

    // File metadata
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

    // Comparison
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

void MusicController::refreshFileMetadata(
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

void MusicController::getDatabaseStats(
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

void MusicController::forceRescan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (scanner_->isInProgress()) {
    ResponseBuilder::sendError(callback, "Rescan already in progress",
                               drogon::k409Conflict);
    return;
  }

  ResponseBuilder::sendSuccess(callback, "Force rescan started in background");

  scanner_->forceRescan([]() {
    // Optional: log completion
  });
}

void MusicController::updateFileTags(
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

void MusicController::deleteAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("album") || !json->isMember("artist")) {
    ResponseBuilder::sendError(callback, "Missing album or artist parameter");
    return;
  }

  std::string albumName = (*json)["album"].asString();
  std::string artistName = (*json)["artist"].asString();
  std::string decodedAlbum = drogon::utils::urlDecode(albumName);
  std::string decodedArtist = drogon::utils::urlDecode(artistName);

  try {
    auto tracks = db_->getTracksByAlbum(decodedAlbum, decodedArtist);
    if (tracks.empty()) {
      ResponseBuilder::sendError(callback, "Album not found",
                                 drogon::k404NotFound);
      return;
    }

    int deletedCount = 0;
    int errorCount = 0;

    for (const auto &track : tracks) {
      if (fs::exists(track.filePath)) {
        std::string trashCmd =
            "kioclient5 move \"" + track.filePath + "\" trash:/ 2>/dev/null";
        if (system(trashCmd.c_str()) == 0) {
          deletedCount++;
        } else {
          errorCount++;
        }
      }
      db_->removeFile(track.filePath);
      cache_->erase(track.filePath);
    }

    Json::Value data;
    data["deleted_files"] = deletedCount;
    data["error_count"] = errorCount;
    data["album"] = decodedAlbum;
    data["artist"] = decodedArtist;

    ResponseBuilder::sendSuccess(callback, data);
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}

void MusicController::getRescanStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  const auto &status = scanner_->getStatus();

  Json::Value response;
  response["in_progress"] = status.inProgress.load();
  response["total_files"] = status.totalFiles.load();
  response["processed_files"] = status.processedFiles.load();
  response["added_files"] = status.addedFiles.load();
  response["error_count"] = status.errorCount.load();
  response["old_albums_count"] = status.oldAlbumsCount.load();
  response["new_albums_count"] = status.newAlbumsCount.load();

  callback(ResponseBuilder::jsonResponse(response));
}
