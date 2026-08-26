#include "PlaylistController.h"
#include <algorithm>
#include <filesystem>

PlaylistController::PlaylistController(App &app,
                                       PlaylistRepository &playlistRepo,
                                       MusicRepository &musicRepo)
    : RestController<App>(app), playlistRepository(playlistRepo),
      musicRepository(musicRepo) {}

void PlaylistController::register_all_routes() {
  this->app_.get("/api/playlists",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetPlaylists(req);
                 });
  this->app_.get("/api/playlists/:name",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetPlaylist(req);
                 });
  this->app_.post("/api/playlists",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleCreatePlaylist(req);
                  });
  this->app_.put("/api/playlists/:name",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleUpdatePlaylist(req);
                 });
  this->app_.del("/api/playlists/:name",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleDeletePlaylist(req);
                 });
  this->app_.post("/api/playlists/:name/rename",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleRenamePlaylist(req);
                  });
  this->app_.post("/api/playlists/:name/tracks",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleAddTracks(req);
                  });
  this->app_.del("/api/playlists/:name/tracks/:index",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleRemoveTrack(req);
                 });
  this->app_.post("/api/playlists/:name/shuffle",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleShufflePlaylist(req);
                  });
  this->app_.post("/api/playlists/:name/clear",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleClearPlaylist(req);
                  });
  this->app_.get("/api/playlists/:name/export",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleExportPlaylist(req);
                 });
  this->app_.post("/api/playlists/import",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleImportPlaylist(req);
                  });
  this->app_.post("/api/playlists/from-artist",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleCreateFromArtist(req);
                  });
  this->app_.post("/api/playlists/from-album",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleCreateFromAlbum(req);
                  });
  this->app_.post("/api/playlists/from-search",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleCreateFromSearch(req);
                  });
  this->app_.post("/api/playlists/validate",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleValidatePlaylists(req);
                  });
  this->app_.post("/api/playlists/scan",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleScanPlaylists(req);
                  });
}

StringHttpResponse
PlaylistController::handleGetPlaylists(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto names = playlistRepository.getAllPlaylistNames();
    nlohmann::json response;
    response["success"] = true;
    response["playlists"] = playlistListToJson(names);
    response["count"] = static_cast<int>(names.size());
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
PlaylistController::handleGetPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    auto playlist = playlistRepository.loadPlaylist(name);
    if (!playlist) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["playlist"] = playlistToJson(*playlist, name);
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
PlaylistController::handleCreatePlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("name")) {
      auto error = this->error_response(400, "Missing required field: name");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string name = body["name"];
    if (playlistRepository.playlistExists(name)) {
      auto error =
          this->error_response(409, "Playlist already exists: " + name);
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    std::vector<std::string> paths;
    if (body.contains("tracks") && body["tracks"].is_array()) {
      for (const auto &track : body["tracks"]) {
        if (track.is_string()) {
          paths.push_back(track);
        }
      }
    }
    Playlist playlist = playlistRepository.createFromFilePaths(paths);
    if (!playlistRepository.savePlaylist(name, playlist)) {
      auto error = this->error_response(500, "Failed to save playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist created successfully";
    response["name"] = name;
    response["track_count"] = static_cast<int>(playlist.size());
    res.setJsonContent(response.dump());
    res.setStatus(201);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
PlaylistController::handleUpdatePlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    if (!playlistRepository.playlistExists(name)) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("tracks") || !body["tracks"].is_array()) {
      auto error =
          this->error_response(400, "Missing or invalid field: tracks");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::vector<std::string> paths;
    for (const auto &track : body["tracks"]) {
      if (track.is_string()) {
        paths.push_back(track);
      }
    }
    Playlist playlist = playlistRepository.createFromFilePaths(paths);
    if (!playlistRepository.savePlaylist(name, playlist)) {
      auto error = this->error_response(500, "Failed to update playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist updated successfully";
    response["name"] = name;
    response["track_count"] = static_cast<int>(playlist.size());
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
PlaylistController::handleDeletePlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    if (!playlistRepository.playlistExists(name)) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.deletePlaylist(name)) {
      auto error = this->error_response(500, "Failed to delete playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist deleted successfully";
    response["name"] = name;
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
PlaylistController::handleRenamePlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string oldName = req.getParam("name");
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("new_name")) {
      auto error =
          this->error_response(400, "Missing required field: new_name");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string newName = body["new_name"];
    if (!playlistRepository.playlistExists(oldName)) {
      auto error = this->error_response(404, "Playlist not found: " + oldName);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    if (playlistRepository.playlistExists(newName)) {
      auto error =
          this->error_response(409, "Playlist already exists: " + newName);
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.renamePlaylist(oldName, newName)) {
      auto error = this->error_response(500, "Failed to rename playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist renamed successfully";
    response["old_name"] = oldName;
    response["new_name"] = newName;
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
PlaylistController::handleAddTracks(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    auto playlist = playlistRepository.loadPlaylist(name);
    if (!playlist) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("tracks") || !body["tracks"].is_array()) {
      auto error =
          this->error_response(400, "Missing or invalid field: tracks");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    Playlist updatedPlaylist(*playlist);
    for (const auto &trackJson : body["tracks"]) {
      if (trackJson.is_string()) {
        std::string path = trackJson;
        auto metadata = musicRepository.getTrack(path);
        if (metadata.has_value()) {
          updatedPlaylist.addTrack(metadata.value());
        } else {
          updatedPlaylist.addTrack(path);
        }
      } else if (trackJson.is_object()) {
        MusicMetadata metadata;
        metadata.filePath = trackJson.value("file_path", "");
        metadata.title = trackJson.value("title", "");
        metadata.artist = trackJson.value("artist", "");
        metadata.album = trackJson.value("album", "");
        metadata.duration = trackJson.value("duration", 0);
        metadata.track = trackJson.value("track", 0);
        metadata.year = trackJson.value("year", 0);
        metadata.genre = trackJson.value("genre", "");
        updatedPlaylist.addTrack(metadata);
      }
    }
    if (!playlistRepository.savePlaylist(name, updatedPlaylist)) {
      auto error = this->error_response(500, "Failed to update playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Tracks added successfully";
    response["name"] = name;
    response["track_count"] = static_cast<int>(updatedPlaylist.size());
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
PlaylistController::handleRemoveTrack(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    int index = std::stoi(req.getParam("index"));
    auto playlist = playlistRepository.loadPlaylist(name);
    if (!playlist) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    if (index < 0 || index >= static_cast<int>(playlist->size())) {
      auto error = this->error_response(400, "Invalid track index: " +
                                                 std::to_string(index));
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    Playlist updatedPlaylist(*playlist);
    updatedPlaylist.removeTrack(index);
    if (!playlistRepository.savePlaylist(name, updatedPlaylist)) {
      auto error = this->error_response(500, "Failed to update playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Track removed successfully";
    response["name"] = name;
    response["track_count"] = static_cast<int>(updatedPlaylist.size());
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
PlaylistController::handleShufflePlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    auto playlist = playlistRepository.loadPlaylist(name);
    if (!playlist) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    Playlist shuffledPlaylist(*playlist);
    shuffledPlaylist.shuffle();
    if (!playlistRepository.savePlaylist(name, shuffledPlaylist)) {
      auto error = this->error_response(500, "Failed to shuffle playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist shuffled successfully";
    response["name"] = name;
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
PlaylistController::handleClearPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    auto playlist = playlistRepository.loadPlaylist(name);
    if (!playlist) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    Playlist clearedPlaylist(std::vector<MusicMetadata>{});
    if (!playlistRepository.savePlaylist(name, clearedPlaylist)) {
      auto error = this->error_response(500, "Failed to clear playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist cleared successfully";
    response["name"] = name;
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
PlaylistController::handleExportPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getParam("name");
    std::string filePath = getQueryParam(req, "path", "");
    if (filePath.empty()) {
      auto error =
          this->error_response(400, "Missing required query parameter: path");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.playlistExists(name)) {
      auto error = this->error_response(404, "Playlist not found: " + name);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.exportToFile(name, filePath)) {
      auto error = this->error_response(500, "Failed to export playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist exported successfully";
    response["name"] = name;
    response["path"] = filePath;
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
PlaylistController::handleImportPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("path")) {
      auto error = this->error_response(400, "Missing required field: path");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string filePath = body["path"];
    if (!std::filesystem::exists(filePath)) {
      auto error = this->error_response(404, "File not found: " + filePath);
      res.setStatus(404);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.importFromFile(filePath)) {
      auto error = this->error_response(500, "Failed to import playlist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string name = std::filesystem::path(filePath).stem().string();
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist imported successfully";
    response["name"] = name;
    response["path"] = filePath;
    res.setJsonContent(response.dump());
    res.setStatus(201);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
PlaylistController::handleCreateFromArtist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("name") || !body.contains("artist")) {
      auto error =
          this->error_response(400, "Missing required fields: name, artist");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string name = body["name"];
    std::string artist = body["artist"];
    if (playlistRepository.playlistExists(name)) {
      auto error =
          this->error_response(409, "Playlist already exists: " + name);
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.createAndSaveFromArtist(name, artist)) {
      auto error =
          this->error_response(500, "Failed to create playlist from artist");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    auto playlist = playlistRepository.loadPlaylist(name);
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist created from artist successfully";
    response["name"] = name;
    response["artist"] = artist;
    response["track_count"] = playlist ? static_cast<int>(playlist->size()) : 0;
    res.setJsonContent(response.dump());
    res.setStatus(201);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
PlaylistController::handleCreateFromAlbum(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("name") || !body.contains("album")) {
      auto error =
          this->error_response(400, "Missing required fields: name, album");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string name = body["name"];
    std::string album = body["album"];
    std::string artist = body.value("artist", "");
    if (playlistRepository.playlistExists(name)) {
      auto error =
          this->error_response(409, "Playlist already exists: " + name);
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.createAndSaveFromAlbum(name, album, artist)) {
      auto error =
          this->error_response(500, "Failed to create playlist from album");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    auto playlist = playlistRepository.loadPlaylist(name);
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist created from album successfully";
    response["name"] = name;
    response["album"] = album;
    response["artist"] = artist;
    response["track_count"] = playlist ? static_cast<int>(playlist->size()) : 0;
    res.setJsonContent(response.dump());
    res.setStatus(201);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
PlaylistController::handleCreateFromSearch(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!body.contains("name") || !body.contains("query")) {
      auto error =
          this->error_response(400, "Missing required fields: name, query");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string name = body["name"];
    std::string query = body["query"];
    if (playlistRepository.playlistExists(name)) {
      auto error =
          this->error_response(409, "Playlist already exists: " + name);
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    if (!playlistRepository.createAndSaveFromSearch(name, query)) {
      auto error =
          this->error_response(500, "Failed to create playlist from search");
      res.setStatus(500);
      res.setJsonContent(error.dump());
      return res;
    }
    auto playlist = playlistRepository.loadPlaylist(name);
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist created from search successfully";
    response["name"] = name;
    response["query"] = query;
    response["track_count"] = playlist ? static_cast<int>(playlist->size()) : 0;
    res.setJsonContent(response.dump());
    res.setStatus(201);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

StringHttpResponse
PlaylistController::handleValidatePlaylists(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = getQueryParam(req, "name", "");
    if (!name.empty()) {
      playlistRepository.validatePlaylist(name);
    } else {
      playlistRepository.validateAllPlaylists();
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlists validated successfully";
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
PlaylistController::handleScanPlaylists(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (playlistRepository.isScanning()) {
      auto error = this->error_response(409, "Scan already in progress");
      res.setStatus(409);
      res.setJsonContent(error.dump());
      return res;
    }
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.getBodyString());
    } catch (...) {
      auto error = this->error_response(400, "Invalid JSON body");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    std::string directory = body.value("directory", "");
    if (directory.empty()) {
      auto error =
          this->error_response(400, "Missing required field: directory");
      res.setStatus(400);
      res.setJsonContent(error.dump());
      return res;
    }
    playlistRepository.scanDirectoryAsync(directory,
                                          [](int total, int processed) {});
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Playlist scan started";
    response["directory"] = directory;
    response["scanning"] = true;
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    auto error = this->error_response(500, e.what());
    res.setStatus(500);
    res.setJsonContent(error.dump());
  }
  return res;
}

nlohmann::json PlaylistController::playlistToJson(const Playlist &playlist,
                                                  const std::string &name) {
  nlohmann::json obj;
  obj["name"] = name;
  obj["current_index"] = playlist.getCurrentIndex();
  obj["track_count"] = static_cast<int>(playlist.size());
  obj["empty"] = playlist.empty();
  nlohmann::json tracksJson = nlohmann::json::array();
  auto tracks = playlist.getAllTracks();
  for (const auto &track : tracks) {
    nlohmann::json trackJson;
    trackJson["file_path"] = track.filePath;
    trackJson["title"] = track.title.empty() ? "Unknown" : track.title;
    trackJson["artist"] = track.artist;
    trackJson["album"] = track.album;
    trackJson["duration"] = track.duration;
    trackJson["track"] = track.track;
    trackJson["year"] = track.year;
    trackJson["genre"] = track.genre;
    tracksJson.push_back(trackJson);
  }
  obj["tracks"] = tracksJson;
  return obj;
}

nlohmann::json
PlaylistController::playlistListToJson(const std::vector<std::string> &names) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto &name : names) {
    nlohmann::json obj;
    obj["name"] = name;
    obj["track_count"] = playlistRepository.getTrackCount(name);
    arr.push_back(obj);
  }
  return arr;
}

std::string PlaylistController::getQueryParam(const StringHttpRequest &req,
                                              const std::string &key,
                                              const std::string &defaultValue) {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

int PlaylistController::getQueryParamInt(const StringHttpRequest &req,
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

bool PlaylistController::getQueryParamBool(const StringHttpRequest &req,
                                           const std::string &key,
                                           bool defaultValue) {
  auto value = req.getQuery(key);
  if (value.empty())
    return defaultValue;
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower == "true" || lower == "1" || lower == "yes";
}
