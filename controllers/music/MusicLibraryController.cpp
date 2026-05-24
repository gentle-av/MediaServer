#include "controllers/music/MusicLibraryController.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>

MusicDatabase *MusicLibraryController::db_ = nullptr;
MetadataCache *MusicLibraryController::cache_ = nullptr;

MusicLibraryController::MusicLibraryController() {}

void MusicLibraryController::init(std::unique_ptr<MusicDatabase> &db,
                                  std::unique_ptr<MetadataCache> &cache) {
  db_ = db.get();
  cache_ = cache.get();
}

void MusicLibraryController::getTracksByArtist(
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

void MusicLibraryController::getTracksByAlbum(
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

void MusicLibraryController::listFiles(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    auto allFiles = db_->getAllFiles();
    Json::Value files(Json::arrayValue);
    for (const auto &filePath : allFiles) {
      if (std::filesystem::exists(filePath)) {
        Json::Value fileInfo;
        fileInfo["path"] = filePath;
        fileInfo["filename"] =
            std::filesystem::path(filePath).filename().string();
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

void MusicLibraryController::getArtists(
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

void MusicLibraryController::getAlbums(
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

void MusicLibraryController::getAlbumsPaginated(
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
