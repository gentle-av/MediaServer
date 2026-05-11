#include "controllers/MusicPlaybackController.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>

std::unique_ptr<PlaylistManager> MusicPlaybackController::playlistManager_ =
    nullptr;

MusicPlaybackController::MusicPlaybackController() {}

void MusicPlaybackController::init(
    std::unique_ptr<MusicDatabase> &db,
    std::shared_ptr<PlayerController> playerController) {
  if (playlistManager_ == nullptr) {
    playlistManager_ = std::make_unique<PlaylistManager>(playerController, *db);
  }
}

void MusicPlaybackController::openMusium(
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

void MusicPlaybackController::openAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  std::string decodedAlbum = drogon::utils::urlDecode(album);
  std::string artistFilter = req->getParameter("artist");
  if (playlistManager_->openMusiumByAlbum(decodedAlbum, artistFilter)) {
    Json::Value data;
    data["album"] = decodedAlbum;
    if (!artistFilter.empty())
      data["artist"] = artistFilter;
    ResponseBuilder::sendSuccess(callback, data);
  } else {
    ResponseBuilder::sendError(callback, "Failed to open album",
                               drogon::k500InternalServerError);
  }
}

void MusicPlaybackController::openArtist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &artist) {
  std::string decodedArtist = drogon::utils::urlDecode(artist);
  if (playlistManager_->openMusiumByArtist(decodedArtist)) {
    Json::Value data;
    data["artist"] = decodedArtist;
    ResponseBuilder::sendSuccess(callback, data);
  } else {
    ResponseBuilder::sendError(callback, "Failed to open artist",
                               drogon::k500InternalServerError);
  }
}
