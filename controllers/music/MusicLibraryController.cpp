#include "MusicLibraryController.h"
#include <algorithm>

MusicLibraryController::MusicLibraryController(
    App &app, MusicRepository &repo, std::shared_ptr<MetadataCache> cache)
    : RestController<App>(app), musicRepository(repo), metadataCache(cache) {}

void MusicLibraryController::register_all_routes() {
  this->app_.get("/api/music/tracks/artist/:artist",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   std::string artist = req.getParam("artist");
                   return this->handleGetTracksByArtist(req, artist);
                 });
  this->app_.get("/api/music/tracks/album/:album",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   std::string album = req.getParam("album");
                   return this->handleGetTracksByAlbum(req, album);
                 });
  this->app_.get("/api/music/list",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   return this->handleListFiles(req);
                 });
  this->app_.get("/api/music/artists",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   return this->handleGetArtists(req);
                 });
  this->app_.get("/api/music/albums",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   return this->handleGetAlbums(req);
                 });
  this->app_.get("/api/music/albums/paginated",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   return this->handleGetAlbumsPaginated(req);
                 });
}

typename App::ResponseType
MusicLibraryController::handleGetTracksByArtist(const App::RequestType &req,
                                                const std::string &artist) {
  App::ResponseType res;
  try {
    std::string cacheKey = "artist_tracks_" + artist;
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
      auto tracks = musicRepository.getTracksByArtist(artist);
      auto response = this->buildTrackResponse(*tracks);
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

typename App::ResponseType
MusicLibraryController::handleGetTracksByAlbum(const App::RequestType &req,
                                               const std::string &album) {
  App::ResponseType res;
  try {
    std::string artistFilter = this->getQueryParam(req, "artist");
    std::string cacheKey = "album_tracks_" + album + "_" + artistFilter;
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
      auto tracks = musicRepository.getTracksByAlbum(album, artistFilter);
      auto response = this->buildTrackResponse(*tracks);
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

typename App::ResponseType
MusicLibraryController::handleListFiles(const App::RequestType &req) {
  App::ResponseType res;
  try {
    std::string cacheKey = "list_files";
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
      auto allFiles = musicRepository.getAllTracks();
      nlohmann::json filesJson = nlohmann::json::array();
      for (const auto &track : *allFiles) {
        if (std::filesystem::exists(track.filePath)) {
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
        } else {
          musicRepository.removeTrack(track.filePath);
        }
      }
      nlohmann::json response;
      response["success"] = true;
      response["files"] = filesJson;
      response["count"] = static_cast<int>(filesJson.size());
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

typename App::ResponseType
MusicLibraryController::handleGetArtists(const App::RequestType &req) {
  App::ResponseType res;
  try {
    std::string cacheKey = "artists_list";
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
      auto artists = musicRepository.getArtists();
      nlohmann::json response;
      response["success"] = true;
      response["artists"] = nlohmann::json(artists);
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

typename App::ResponseType
MusicLibraryController::handleGetAlbums(const App::RequestType &req) {
  App::ResponseType res;
  try {
    std::string artistFilter = this->getQueryParam(req, "artist");
    std::string cacheKey = "albums_list_" + artistFilter;
    std::string cached = this->get_cached_or_generate(cacheKey, [&]() {
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

typename App::ResponseType
MusicLibraryController::handleGetAlbumsPaginated(const App::RequestType &req) {
  App::ResponseType res;
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
MusicLibraryController::getQueryParam(const App::RequestType &req,
                                      const std::string &key,
                                      const std::string &defaultValue) {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

int MusicLibraryController::getQueryParamInt(const App::RequestType &req,
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
