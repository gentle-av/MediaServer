#include "controllers/player/PlayerController.h"
#include <drogon/utils/Utilities.h>

PlayerController::PlayerController() {
  sessionManager_ = std::make_unique<PlayerSessionManager>();
  commandSender_ = std::make_unique<MpvCommandSender>("");
  trackLoader_ = std::make_unique<TrackLoader>([this](const std::string &cmd) {
    return commandSender_->sendCommand(cmd);
  });
  tracklistManager_ = std::make_unique<TrackListManager>(
      [this](int index) { loadTrack(index); });
  autoAdvanceTracker_ = std::make_unique<AutoAdvanceTracker>(
      [this](const std::string &cmd) {
        return commandSender_->sendCommand(cmd);
      },
      [this](int index) { loadTrack(index); });
  volumeController_ = std::make_unique<Volumer>();
  outputSwitcher_ = std::make_unique<AudioOutputSwitcher>();
  system("amixer set Master 45% 2>/dev/null");
}

PlayerController::~PlayerController() {
  stopAutoAdvance_ = true;
  if (autoAdvanceTracker_)
    autoAdvanceTracker_->stop();
  if (idleTimerThread_ && idleTimerThread_->joinable())
    idleTimerThread_->join();
  auto tracks = tracklistManager_->getTrackList();
  int currentIdx = currentIndex_.load();
  sessionManager_->stopMpv(socketPath_, tracks, currentIdx, isPlaying_);
}

void PlayerController::startMpvIfNeeded() {
  if (!sessionManager_->isProcessAlive(socketPath_)) {
    if (!socketPath_.empty())
      socketPath_.clear();
    socketPath_ = sessionManager_->generateSocketPath();
    sessionManager_->launchMpv(socketPath_);
    commandSender_->setSocketPath(socketPath_);
    if (!autoAdvanceTracker_->isRunning()) {
      stopAutoAdvance_ = false;
      auto tracks = tracklistManager_->getTrackList();
      autoAdvanceTracker_->start(stopAutoAdvance_, isPlaying_, tracks,
                                 currentIndex_);
    }
    resetIdleTimer();
  }
}

void PlayerController::loadTrack(int index) {
  if (!tracklistManager_->hasTrack(index))
    return;
  currentIndex_ = index;
  isPlaying_ = true;
  trackLoader_->loadTrack(tracklistManager_->getTrack(index), currentIndex_,
                          isPlaying_);
  resetIdleTimer();
}

void PlayerController::resetIdleTimer() {
  if (!sessionManager_->isProcessAlive(socketPath_))
    return;
}

Json::Value PlayerController::parseBody(const drogon::HttpRequestPtr &req) {
  auto body = req->getBody();
  Json::Value result;
  if (body.empty())
    return result;
  Json::CharReaderBuilder builder;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(body.data(), body.data() + body.size(), &result, &errors);
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
  startMpvIfNeeded();
  commandSender_->sendCommand(
      R"({"command": ["set_property", "pause", false]})");
  isPlaying_ = true;
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started")));
}

void PlayerController::handlePause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  commandSender_->sendCommand(
      R"({"command": ["set_property", "pause", true]})");
  isPlaying_ = false;
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused")));
}

void PlayerController::handleStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  commandSender_->sendCommand(R"({"command": ["stop"]})");
  currentIndex_ = -1;
  isPlaying_ = false;
  resetIdleTimer();
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Stopped")));
}

void PlayerController::handleNext(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  if (currentIndex_ + 1 < tracklistManager_->size())
    loadTrack(currentIndex_ + 1);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Next track")));
}

void PlayerController::handlePrevious(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  if (currentIndex_ - 1 >= 0)
    loadTrack(currentIndex_ - 1);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Previous track")));
}

void PlayerController::handleSetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("tracks") || !json["tracks"].isArray()) {
      callback(drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing tracks array parameter")));
      return;
    }
    std::vector<std::string> tracks;
    for (const auto &track : json["tracks"]) {
      if (track.isString())
        tracks.push_back(drogon::utils::urlDecode(track.asString()));
    }
    tracklistManager_->setTrackList(tracks);
    if (!tracks.empty()) {
      startMpvIfNeeded();
      loadTrack(0);
    }
    resetIdleTimer();
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playlist set")));
  } catch (const std::exception &e) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what())));
  }
}

void PlayerController::handleAddToPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  auto json = parseBody(req);
  if (!json.isMember("track") || !json["track"].isString()) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Missing track parameter")));
    return;
  }
  tracklistManager_->addTrack(
      drogon::utils::urlDecode(json["track"].asString()));
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Track added")));
}

void PlayerController::handleClear(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  commandSender_->sendCommand(R"({"command": ["stop"]})");
  tracklistManager_->clearTrackList();
  currentIndex_ = -1;
  isPlaying_ = false;
  resetIdleTimer();
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Cleared")));
}

void PlayerController::handleGetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value playlist(Json::arrayValue);
  for (const auto &track : tracklistManager_->getTrackList())
    playlist.append(track);
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "", playlist)));
}

void PlayerController::handlePlaybackState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value state;
  if (!sessionManager_->isProcessAlive(socketPath_)) {
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex_.load();
    state["totalTracks"] = tracklistManager_->size();
    state["currentTime"] = 0;
    state["duration"] = 0;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", state)));
    return;
  }
  std::string pauseResp =
      commandSender_->sendCommand(R"({"command": ["get_property", "pause"]})");
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  std::string timeResp = commandSender_->sendCommand(
      R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp = commandSender_->sendCommand(
      R"({"command": ["get_property", "duration"]})");
  double currentTime = 0, duration = 0;
  size_t pos = timeResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = timeResp.find(":", pos);
    if (start != std::string::npos) {
      try {
        currentTime = std::stod(timeResp.substr(start + 1));
      } catch (...) {
      }
    }
  }
  pos = durationResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = durationResp.find(":", pos);
    if (start != std::string::npos) {
      try {
        duration = std::stod(durationResp.substr(start + 1));
      } catch (...) {
      }
    }
  }
  state["isPlaying"] = !isPaused && (currentTime > 0 || duration > 0);
  state["currentTrack"] =
      (currentIndex_ >= 0 && currentIndex_ < tracklistManager_->size())
          ? tracklistManager_->getTrack(currentIndex_)
          : "";
  state["currentIndex"] = currentIndex_.load();
  state["totalTracks"] = tracklistManager_->size();
  state["currentTime"] = currentTime;
  state["duration"] = duration > 0 ? duration : 300;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", state)));
}

void PlayerController::handleGetCurrentTime(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value data;
  if (!sessionManager_->isProcessAlive(socketPath_)) {
    data["currentTime"] = 0;
    data["duration"] = 0;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", data)));
    return;
  }
  std::string timeResp = commandSender_->sendCommand(
      R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp = commandSender_->sendCommand(
      R"({"command": ["get_property", "duration"]})");
  double currentTime = 0, duration = 0;
  try {
    Json::Value timeJson, durationJson;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (reader->parse(timeResp.c_str(), timeResp.c_str() + timeResp.size(),
                      &timeJson, &errors) &&
        timeJson.isMember("data") && timeJson["data"].isNumeric()) {
      currentTime = timeJson["data"].asDouble();
    }
    if (reader->parse(durationResp.c_str(),
                      durationResp.c_str() + durationResp.size(), &durationJson,
                      &errors) &&
        durationJson.isMember("data") && durationJson["data"].isNumeric()) {
      duration = durationJson["data"].asDouble();
    }
  } catch (...) {
  }
  data["currentTime"] = currentTime;
  data["duration"] = duration > 0 ? duration : 0;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data)));
}

void PlayerController::handleSeek(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  auto json = parseBody(req);
  if (!json.isMember("position")) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Missing position parameter")));
    return;
  }
  double position = std::max(0.0, json["position"].asDouble());
  commandSender_->sendCommand(R"({"command": ["seek", )" +
                              std::to_string(position) + R"(, "absolute"]})");
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Seek completed")));
}

void PlayerController::handlePlayFile(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = parseBody(req);
  if (!json.isMember("path") || !json["path"].isString()) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Missing path parameter")));
    return;
  }
  std::string path = drogon::utils::urlDecode(json["path"].asString());
  tracklistManager_->clearTrackList();
  tracklistManager_->addTrack(path);
  startMpvIfNeeded();
  loadTrack(0);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playing file")));
}

void PlayerController::handleGetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  int volume = volumeController_->getVolume();
  if (volume < 0) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to get volume")));
    return;
  }
  Json::Value data;
  data["volume"] = volume;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data)));
}

void PlayerController::handleSetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("volume") || !json["volume"].isInt()) {
      callback(drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing volume parameter")));
      return;
    }
    int volume = json["volume"].asInt();
    if (volume < 0 || volume > 100) {
      callback(drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Volume must be 0-100")));
      return;
    }
    volumeController_->setVolume(volume);
    Json::Value data;
    data["volume"] = volume;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", data)));
  } catch (const std::exception &e) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what())));
  }
}

void PlayerController::handleIncreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  volumeController_->increaseVolume();
  Json::Value data;
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Volume increased", data)));
}

void PlayerController::handleDecreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  volumeController_->decreaseVolume();
  Json::Value data;
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Volume decreased", data)));
}

void PlayerController::handleToggleMute(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  volumeController_->toggleMute();
  Json::Value data;
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Toggled mute", data)));
}

void PlayerController::handleSwitchToSpeakers(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (outputSwitcher_->switchToSpeakers()) {
    Json::Value data;
    data["output"] = "speakers";
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Switched to speakers", data)));
  } else {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to switch")));
  }
}

void PlayerController::handleSwitchToHeadphones(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (outputSwitcher_->switchToHeadphones()) {
    Json::Value data;
    data["output"] = "headphones";
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Switched to headphones", data)));
  } else {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to switch")));
  }
}

void PlayerController::handleGetAudioOutput(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value data;
  data["current"] = outputSwitcher_->getCurrentOutput();
  Json::Value available(Json::arrayValue);
  for (const auto &output : outputSwitcher_->getAvailableOutputs())
    available.append(output);
  data["available"] = available;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data)));
}

void PlayerController::handlePlayIndex(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  auto json = parseBody(req);
  if (!json.isMember("index") || !json["index"].isInt()) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Missing index parameter")));
    return;
  }
  int index = json["index"].asInt();
  if (index < 0 || index >= tracklistManager_->size()) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Index out of range")));
    return;
  }
  loadTrack(index);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playing track at index " + std::to_string(index))));
}

void PlayerController::handleForceStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  stopAutoAdvance_ = true;
  if (autoAdvanceTracker_)
    autoAdvanceTracker_->stop();
  if (idleTimerThread_ && idleTimerThread_->joinable())
    idleTimerThread_->join();
  auto tracks = tracklistManager_->getTrackList();
  int currentIdx = currentIndex_.load();
  sessionManager_->stopMpv(socketPath_, tracks, currentIdx, isPlaying_);
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Audio force stopped")));
}
