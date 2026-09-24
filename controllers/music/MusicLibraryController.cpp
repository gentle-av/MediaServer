#include "MusicLibraryController.h"
#include <algorithm>

MusicLibraryController::MusicLibraryController(
    App &app, MusicRepository &repo, std::shared_ptr<MetadataCache> cache)
    : RestController<App>(app), musicRepository(repo), metadataCache(cache) {
  this->musicRepository.getEventBus().subscribe(
      "cacheinvalidated", [this](const std::string &) { this->clear_cache(); });
}

void MusicLibraryController::register_all_routes() {
  this->app_.get("/api/music/tracks/artist/:artist",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   std::string artist = req.getParam("artist");
                   return this->handleGetTracksByArtist(req, artist);
                 });
  this->app_.get("/api/music/tracks/album/:album",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   std::string album = req.getParam("album");
                   return this->handleGetTracksByAlbum(req, album);
                 });
  this->app_.get("/api/music/list",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleListFiles(req);
                 });
  this->app_.get("/api/music/artists",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetArtists(req);
                 });
  this->app_.get("/api/music/albums",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetAlbums(req);
                 });
  this->app_.get("/api/music/albums/paginated",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetAlbumsPaginated(req);
                 });
}

StringHttpResponse
MusicLibraryController::handleGetTracksByArtist(const StringHttpRequest &req,
                                                const std::string &artist) {
  StringHttpResponse res;
  try {
    auto tracks = musicRepository.getTracksByArtist(artist);
    auto response = this->buildTrackResponse(*tracks);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
MusicLibraryController::handleGetTracksByAlbum(const StringHttpRequest &req,
                                               const std::string &album) {
  StringHttpResponse res;
  try {
    std::string artistFilter = this->getQueryParam(req, "artist");
    auto tracks = musicRepository.getTracksByAlbum(album, artistFilter);
    auto response = this->buildTrackResponse(*tracks);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
MusicLibraryController::handleListFiles(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto allFiles = musicRepository.getAllTracks();
    std::cerr << "[MUSIC_LIST] repository size=" << allFiles->size()
              << std::endl;
    nlohmann::json filesJson = nlohmann::json::array();
    int skippedNotOnDisk = 0;
    for (const auto &track : *allFiles) {
      if (!std::filesystem::exists(track.filePath)) {
        skippedNotOnDisk++;
        continue;
      }
      nlohmann::json fileInfo;
      fileInfo["path"] = track.filePath;
      fileInfo["filename"] =
          std::filesystem::path(track.filePath).filename().string();
      fileInfo["title"] = track.title;
      fileInfo["artist"] = track.artist;
      fileInfo["album"] = track.album;
      fileInfo["duration"] = track.duration;
      fileInfo["track"] = track.track;
      fileInfo["year"] = track.year;
      fileInfo["genre"] = track.genre;
      filesJson.push_back(fileInfo);
    }
    std::cerr << "[MUSIC_LIST] emitted=" << filesJson.size()
              << " skippedNotOnDisk=" << skippedNotOnDisk << std::endl;
    nlohmann::json response;
    response["success"] = true;
    response["files"] = filesJson;
    response["count"] = static_cast<int>(filesJson.size());
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    std::cerr << "[MUSIC_LIST] exception: " << e.what() << std::endl;
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
MusicLibraryController::handleGetArtists(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto artists = musicRepository.getArtists();
    nlohmann::json response;
    response["success"] = true;
    response["artists"] = nlohmann::json(artists);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
MusicLibraryController::handleGetAlbums(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string artistFilter = this->getQueryParam(req, "artist");
    auto albums = musicRepository.getAlbums(artistFilter);
    nlohmann::json albumsJson = nlohmann::json::array();
    for (const auto &[album, artist, year] : albums) {
      nlohmann::json albumObj;
      albumObj["album"] = album;
      albumObj["artist"] = artist;
      albumObj["year"] = year;
      albumsJson.push_back(albumObj);
    }
    nlohmann::json response;
    response["success"] = true;
    response["albums"] = albumsJson;
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
MusicLibraryController::handleGetAlbumsPaginated(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string artistFilter = this->getQueryParam(req, "artist");
    int page = this->getQueryParamInt(req, "page", 1);
    int pageSize = this->getQueryParamInt(req, "pageSize", 20);
    if (pageSize > 50)
      pageSize = 50;
    if (page < 1)
      page = 1;
    std::string cacheKey = "albums_paginated_" + artistFilter + "_" +
                           std::to_string(page) + "_" +
                           std::to_string(pageSize);
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
      auto allAlbums = musicRepository.getAlbums(artistFilter);
      int totalCount = static_cast<int>(allAlbums.size());
      int totalPages = (totalCount + pageSize - 1) / pageSize;
      int offset = (page - 1) * pageSize;
      int start = offset;
      int end = std::min(offset + pageSize, totalCount);
      nlohmann::json albumsJson = nlohmann::json::array();
      for (int i = start; i < end; ++i) {
        const auto &[album, artist, year] = allAlbums[i];
        nlohmann::json albumObj;
        albumObj["album"] = album;
        albumObj["artist"] = artist;
        albumObj["year"] = year;
        albumsJson.push_back(albumObj);
      }
      nlohmann::json response;
      response["success"] = true;
      response["albums"] = albumsJson;
      response["pagination"]["currentPage"] = page;
      response["pagination"]["pageSize"] = pageSize;
      response["pagination"]["totalCount"] = totalCount;
      response["pagination"]["totalPages"] = totalPages;
      response["pagination"]["hasNext"] = page < totalPages;
      response["pagination"]["hasPrev"] = page > 1;
      return response.dump();
    });
    res.setJsonContent(cached);
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

nlohmann::json MusicLibraryController::trackToJson(const MusicMetadata &track) {
  nlohmann::json obj;
  obj["path"] = track.filePath;
  obj["title"] = track.title.empty() ? "Unknown" : track.title;
  obj["artist"] = track.artist;
  obj["album"] = track.album;
  obj["duration"] = track.duration;
  obj["track"] = track.track;
  obj["year"] = track.year;
  obj["genre"] = track.genre;
  return obj;
}

nlohmann::json MusicLibraryController::buildTrackResponse(
    const std::vector<MusicMetadata> &tracks) {
  nlohmann::json tracksJson = nlohmann::json::array();
  for (const auto &track : tracks) {
    tracksJson.push_back(this->trackToJson(track));
  }
  nlohmann::json response;
  response["success"] = true;
  response["tracks"] = tracksJson;
  response["count"] = static_cast<int>(tracks.size());
  return response;
}

std::string
MusicLibraryController::getQueryParam(const StringHttpRequest &req,
                                      const std::string &key,
                                      const std::string &defaultValue) {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

int MusicLibraryController::getQueryParamInt(const StringHttpRequest &req,
                                             const std::string &key,
                                             int defaultValue) {
  auto value = req.getQuery(key);
  if (value.empty())
    return defaultValue;
  try {
    return std::stoi(value);
  } catch (...) {
    return defaultValue;
  }
}
