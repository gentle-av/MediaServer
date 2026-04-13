#include "PlayerController.h"
#include "player/Player.h"

std::shared_ptr<PlayerService> PlayerController::playerService_ = nullptr;

PlayerController::PlayerController() {}

void PlayerController::setPlayerService(
    std::shared_ptr<PlayerService> service) {
  playerService_ = service;
}

Json::Value PlayerController::parseBody(const drogon::HttpRequestPtr &req) {
  auto body = req->getBody();
  Json::Value result;
  if (body.empty())
    return result;
  std::string bodyStr(body.data(), body.size());
  Json::CharReaderBuilder builder;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(bodyStr.c_str(), bodyStr.c_str() + bodyStr.size(), &result,
                &errors);
  return result;
}

Json::Value PlayerController::jsonResponse(bool success,
                                           const std::string &message,
                                           const Json::Value &data) {
  Json::Value resp;
  resp["success"] = success;
  if (!message.empty())
    resp["message"] = message;
  if (!data.empty())
    resp["data"] = data;
  return resp;
}

void PlayerController::handlePlay(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->play();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started"));
  callback(resp);
}

void PlayerController::handlePause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->pause();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused"));
  callback(resp);
}

void PlayerController::handleStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->stop();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback stopped"));
  callback(resp);
}

void PlayerController::handleNext(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->next();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Next track"));
  callback(resp);
}

void PlayerController::handlePrevious(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->previous();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Previous track"));
  callback(resp);
}

void PlayerController::handleSetTrack(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("track") || !json["track"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing track parameter"));
      callback(resp);
      return;
    }
    std::string track = json["track"].asString();
    playerService_->replacePlaylistWithTrack(track);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track set and playing"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleSetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("tracks") || !json["tracks"].isArray()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing tracks array parameter"));
      callback(resp);
      return;
    }
    std::vector<std::string> tracks;
    for (const auto &track : json["tracks"]) {
      if (track.isString()) {
        tracks.push_back(track.asString());
      }
    }
    playerService_->replacePlaylist(tracks);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playlist set"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleAddToPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("track") || !json["track"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing track parameter"));
      callback(resp);
      return;
    }
    playerService_->addToPlaylist(json["track"].asString());
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track added to playlist"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleAddAfterCurrent(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("track") || !json["track"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing track parameter"));
      callback(resp);
      return;
    }
    playerService_->addAfterCurrent(json["track"].asString());
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track added after current"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handlePlayIndex(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("index") || !json["index"].isInt()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing index parameter"));
      callback(resp);
      return;
    }
    playerService_->playIndex(json["index"].asInt());
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playing track at index"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleClear(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  playerService_->clear();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playlist cleared"));
  callback(resp);
}

void PlayerController::handleGetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  Json::Value playlist = playerService_->getPlaylist();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "", playlist));
  callback(resp);
}

void PlayerController::handleGetPlaybackState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  Json::Value state = playerService_->getPlaybackState();
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", state));
  callback(resp);
}

void PlayerController::handleGetCurrentTrack(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  Json::Value track = playerService_->getCurrentTrack();
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", track));
  callback(resp);
}

void PlayerController::handleGetCurrentTime(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  Json::Value time = playerService_->sendRequest("/api/currentTime", "GET");
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", time));
  callback(resp);
}

void PlayerController::handleRemoveFromPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("index") || !json["index"].isInt()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing index parameter"));
      callback(resp);
      return;
    }
    int index = json["index"].asInt();
    playerService_->removeFromPlaylist(index);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track removed from playlist"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleSeek(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (!playerService_ || !playerService_->isAvailable()) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Player service not available"));
    callback(resp);
    return;
  }
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("position") || !json["position"].isDouble()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing position parameter"));
      callback(resp);
      return;
    }
    double position = json["position"].asDouble();
    if (playerService_->useInternalPlayer()) {
      auto player = playerService_->getInternalPlayer();
      if (player) {
        player->seekTo(position);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            jsonResponse(true, "Seek completed"));
        callback(resp);
        return;
      }
    }
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Seek failed"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}
