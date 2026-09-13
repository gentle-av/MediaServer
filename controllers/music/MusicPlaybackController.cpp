#include "controllers/music/MusicPlaybackController.h"

MusicPlaybackController::MusicPlaybackController(
    App &app, std::shared_ptr<MusicDatabase> db,
    std::shared_ptr<PlayerController> playerController)
    : RestController<App>(app), db(db), playerController(playerController) {}

void MusicPlaybackController::register_all_routes() {
  this->app_.post("/api/music/open",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleOpenMusium(req);
                  });
  this->app_.post("/api/music/open/album/:album",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleOpenAlbum(req);
                  });
  this->app_.post("/api/music/open/artist/:artist",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleOpenArtist(req);
                  });
}

StringHttpResponse
MusicPlaybackController::handleOpenMusium(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(req.getBody());
  } catch (...) {
    res.setStatus(400);
    res.setJsonContent(this->error_response(400, "Invalid JSON").dump());
    return res;
  }
  if (!json.contains("tracks") || !json["tracks"].is_array()) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Missing tracks array").dump());
    return res;
  }
  std::vector<std::string> tracks;
  for (const auto &track : json["tracks"]) {
    if (track.is_string()) {
      tracks.push_back(track.get<std::string>());
    }
  }
  if (tracks.empty()) {
    res.setStatus(400);
    res.setJsonContent(this->error_response(400, "No tracks provided").dump());
    return res;
  }
  nlohmann::json playlistBody;
  playlistBody["tracks"] = tracks;
  StringHttpRequest mockReq;
  mockReq.setBody(playlistBody.dump());
  StringHttpResponse playerRes = playerController->handleSetPlaylist(mockReq);
  if (playerRes.getStatus() == 200) {
    nlohmann::json data = this->success_response("Musium opened");
    data["tracks_count"] = static_cast<int>(tracks.size());
    res.setJsonContent(data.dump());
    res.setStatus(200);
  } else {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, "Player error").dump());
  }
  return res;
}

StringHttpResponse
MusicPlaybackController::handleOpenAlbum(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string album = req.getParam("album");
  std::string artistFilter = this->getQueryParam(req, "artist");
  auto trackMetadata = db->getTracksByAlbumRaw(album, artistFilter);
  if (trackMetadata.empty()) {
    res.setStatus(404);
    res.setJsonContent(this->error_response(404, "Album empty or dead").dump());
    return res;
  }
  std::vector<std::string> tracks;
  tracks.reserve(trackMetadata.size());
  for (const auto &meta : trackMetadata) {
    tracks.push_back(meta.filePath);
  }
  nlohmann::json playlistBody;
  playlistBody["tracks"] = tracks;
  StringHttpRequest mockReq;
  mockReq.setBody(playlistBody.dump());
  StringHttpResponse playerRes = playerController->handleSetPlaylist(mockReq);
  if (playerRes.getStatus() == 200) {
    nlohmann::json data = this->success_response("Album opened");
    data["album"] = album;
    if (!artistFilter.empty()) {
      data["artist"] = artistFilter;
    }
    res.setJsonContent(data.dump());
    res.setStatus(200);
  } else {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, "Player error").dump());
  }
  return res;
}

StringHttpResponse
MusicPlaybackController::handleOpenArtist(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string artist = req.getParam("artist");
  auto trackMetadata = db->getTracksByArtistRaw(artist);
  if (trackMetadata.empty()) {
    res.setStatus(404);
    res.setJsonContent(
        this->error_response(404, "Artist has no tracks").dump());
    return res;
  }
  std::vector<std::string> tracks;
  tracks.reserve(trackMetadata.size());
  for (const auto &meta : trackMetadata) {
    tracks.push_back(meta.filePath);
  }
  nlohmann::json playlistBody;
  playlistBody["tracks"] = tracks;
  StringHttpRequest mockReq;
  mockReq.setBody(playlistBody.dump());
  StringHttpResponse playerRes = playerController->handleSetPlaylist(mockReq);
  if (playerRes.getStatus() == 200) {
    nlohmann::json data = this->success_response("Artist opened");
    data["artist"] = artist;
    res.setJsonContent(data.dump());
    res.setStatus(200);
  } else {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, "Player error").dump());
  }
  return res;
}

std::string
MusicPlaybackController::getQueryParam(const StringHttpRequest &req,
                                       const std::string &key,
                                       const std::string &defaultValue) {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}
