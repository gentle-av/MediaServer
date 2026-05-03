// PlayerController.cpp
#include "PlayerController.h"
#include "services/AlsaMixer.h"
#include <chrono>
#include <drogon/utils/Utilities.h>
#include <regex>
#include <thread>
#include <unistd.h>

void PlayerController::launchMpv() {
  if (socketPath_.empty())
    return;
  unlink(socketPath_.c_str());
  std::string cmd = "mpv --input-ipc-server=" + socketPath_ +
                    " --idle --no-video --ao=alsa --no-terminal --really-quiet "
                    "> /dev/null 2>&1 &";
  system(cmd.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void PlayerController::stopMpv() {
  if (!socketPath_.empty()) {
    std::string quitCmd = "timeout 2 sh -c 'echo \"{\\\"command\\\": "
                          "[\\\"quit\\\"]}\" | socat - " +
                          socketPath_ + " 2>/dev/null'";
    system(quitCmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    system(("pkill -f 'mpv.*" + socketPath_ + "' 2>/dev/null").c_str());
    system(("rm -f " + socketPath_ + " 2>/dev/null").c_str());
    socketPath_.clear();
  }
  playlist_.clear();
  currentIndex_ = -1;
  isPlaying_ = false;
}

PlayerController::~PlayerController() {
  stopAutoAdvance_ = true;
  if (autoAdvanceThread_ && autoAdvanceThread_->joinable())
    autoAdvanceThread_->join();
  if (idleTimerThread_ && idleTimerThread_->joinable())
    idleTimerThread_->join();
  stopMpv();
}

void PlayerController::scheduleStop() {
  std::lock_guard<std::mutex> lock(timerMutex_);
  if (idleTimerThread_ && idleTimerThread_->joinable()) {
    idleTimerThread_->join();
    idleTimerThread_.reset();
  }
  idleTimerThread_ = std::make_unique<std::thread>([this]() {
    for (int i = 0; i < 180; i++) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (isPlaying_ || !playlist_.empty() || currentIndex_ != -1 ||
          stopAutoAdvance_)
        return;
    }
    if (!isPlaying_ && playlist_.empty() && currentIndex_ == -1 &&
        !stopAutoAdvance_) {
      if (autoAdvanceThread_ && autoAdvanceThread_->joinable()) {
        stopAutoAdvance_ = true;
        autoAdvanceThread_->join();
        autoAdvanceThread_.reset();
        stopAutoAdvance_ = false;
      }
      stopMpv();
    }
  });
}

void PlayerController::resetIdleTimer() {
  if (!isProcessAlive())
    return;
  scheduleStop();
}

void PlayerController::ensureMpvRunning() {
  if (isProcessAlive())
    resetIdleTimer();
}

void PlayerController::startMpvIfNeeded() {
  if (!isProcessAlive()) {
    if (!socketPath_.empty())
      socketPath_.clear();
    socketPath_ = "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
                  std::to_string(instanceCounter_++);
    launchMpv();
    if (!autoAdvanceThread_) {
      stopAutoAdvance_ = false;
      autoAdvanceThread_ = std::make_unique<std::thread>([this]() {
        while (!stopAutoAdvance_) {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          if (stopAutoAdvance_ || !isProcessAlive() || currentIndex_ < 0 ||
              currentIndex_ >= (int)playlist_.size())
            continue;
          std::string eofResp =
              sendCommand("{\"command\": [\"get_property\", \"eof-reached\"]}");
          bool eofReached = eofResp.find("\"data\":true") != std::string::npos;
          if (!eofReached) {
            std::string timeResp =
                sendCommand("{\"command\": [\"get_property\", \"time-pos\"]}");
            std::string durationResp =
                sendCommand("{\"command\": [\"get_property\", \"duration\"]}");
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
            if (duration == 0)
              duration = 300;
            if (duration > 0 && (duration - currentTime) < 0.5)
              eofReached = true;
          }
          if (eofReached && currentIndex_ + 1 < (int)playlist_.size())
            loadTrack(currentIndex_ + 1);
        }
      });
    }
    resetIdleTimer();
  }
}

std::string PlayerController::escapePath(const std::string &path) {
  std::string escaped = path;
  size_t pos = 0;
  while ((pos = escaped.find("\"", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return escaped;
}

void PlayerController::loadTrack(int index) {
  if (index < 0 || index >= (int)playlist_.size())
    return;
  currentIndex_ = index;
  isPlaying_ = true;
  std::string cmd = "{\"command\": [\"loadfile\", \"" +
                    escapePath(playlist_[index]) + "\", \"replace\"]}";
  sendCommand(cmd);
  sendCommand("{\"command\": [\"set_property\", \"pause\", false]}");
  resetIdleTimer();
}

std::string PlayerController::sendCommand(const std::string &jsonCmd) {
  if (socketPath_.empty() || !isProcessAlive())
    return "";
  std::string cmd = "echo '" + jsonCmd + "' | socat - " + socketPath_ + " 2>&1";
  std::array<char, 512> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return result;
}

bool PlayerController::isProcessAlive() {
  if (socketPath_.empty() || access(socketPath_.c_str(), F_OK) != 0)
    return false;
  std::string cmd = "timeout 1 pgrep -f 'mpv.*" + socketPath_ + "' 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return !result.empty();
}

int PlayerController::instanceCounter_ = 0;

PlayerController::PlayerController() : currentIndex_(-1) {
  system("amixer set Master 45% 2>/dev/null");
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
  sendCommand(R"({"command": ["set_property", "pause", false]})");
  isPlaying_ = true;
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started")));
}

void PlayerController::handlePause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["set_property", "pause", true]})");
  isPlaying_ = false;
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused")));
}

void PlayerController::handleStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["stop"]})");
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
  if (currentIndex_ + 1 < (int)playlist_.size())
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
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing tracks array parameter"));
      callback(resp);
      return;
    }
    playlist_.clear();
    for (const auto &track : json["tracks"]) {
      if (track.isString())
        playlist_.push_back(drogon::utils::urlDecode(track.asString()));
    }
    if (!playlist_.empty()) {
      startMpvIfNeeded();
      loadTrack(0);
    }
    resetIdleTimer();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playlist set"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleGetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value playlist(Json::arrayValue);
  for (const auto &track : playlist_)
    playlist.append(track);
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "", playlist)));
}

void PlayerController::handlePlaybackState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value state;
  if (!isProcessAlive()) {
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex_;
    state["totalTracks"] = (int)playlist_.size();
    state["currentTime"] = 0;
    state["duration"] = 0;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", state)));
    return;
  }
  std::string pauseResp =
      sendCommand(R"({"command": ["get_property", "pause"]})");
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp =
      sendCommand(R"({"command": ["get_property", "duration"]})");
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
      (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size())
          ? playlist_[currentIndex_]
          : "";
  state["currentIndex"] = currentIndex_;
  state["totalTracks"] = (int)playlist_.size();
  state["currentTime"] = currentTime;
  state["duration"] = duration > 0 ? duration : 300;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", state)));
}

void PlayerController::handleForceStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  stopMpv();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Audio force stopped")));
}

void PlayerController::handleGetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string cmd = "timeout 2 amixer get Master 2>/dev/null";
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to execute amixer")));
    return;
  }
  while (fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  pclose(pipe);
  int volume = -1;
  std::regex volumeRegex(R"((\d+)%)");
  std::smatch match;
  if (std::regex_search(result, match, volumeRegex))
    volume = std::stoi(match[1].str());
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
    int amixerValue = 135 + (volume * (255 - 135) / 100);
    system(("timeout 2 amixer set Master " + std::to_string(amixerValue) +
            " 2>/dev/null")
               .c_str());
    Json::Value data;
    data["volume"] = volume;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", data)));
  } catch (const std::exception &e) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what())));
  }
}

void PlayerController::handleToggleMute(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  system("timeout 2 amixer set Master toggle 2>/dev/null");
  Json::Value data;
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Toggled mute", data)));
}

void PlayerController::handleSwitchToSpeakers(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (AlsaMixer::getInstance().switchToSpeakers()) {
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
  if (AlsaMixer::getInstance().switchToHeadphones()) {
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
  std::string currentOutput = AlsaMixer::getInstance().getCurrentOutput();
  if (currentOutput.empty())
    currentOutput = "speakers";
  data["current"] = currentOutput;
  Json::Value available(Json::arrayValue);
  for (const auto &output : AlsaMixer::getInstance().getAvailableOutputs())
    available.append(output);
  data["available"] = available;
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data)));
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
  playlist_.push_back(drogon::utils::urlDecode(json["track"].asString()));
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Track added")));
}

void PlayerController::handleClear(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["stop"]})");
  playlist_.clear();
  currentIndex_ = -1;
  isPlaying_ = false;
  resetIdleTimer();
  callback(
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Cleared")));
}

void PlayerController::handleGetCurrentTime(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value data;
  if (!isProcessAlive()) {
    data["currentTime"] = 0;
    data["duration"] = 0;
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", data)));
    return;
  }
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp =
      sendCommand(R"({"command": ["get_property", "duration"]})");
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
  sendCommand(R"({"command": ["seek", )" + std::to_string(position) +
              R"(, "absolute"]})");
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
  playlist_.clear();
  playlist_.push_back(path);
  startMpvIfNeeded();
  loadTrack(0);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playing file")));
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
  if (index < 0 || index >= (int)playlist_.size()) {
    callback(drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Index out of range")));
    return;
  }
  loadTrack(index);
  resetIdleTimer();
  callback(drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playing track at index " + std::to_string(index))));
}

void PlayerController::handleIncreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  system("timeout 2 amixer set Master 5%+ 2>/dev/null");
  Json::Value data;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Volume increased", data));
  callback(std::move(resp));
}

void PlayerController::handleDecreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  system("timeout 2 amixer set Master 5%- 2>/dev/null");
  Json::Value data;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Volume decreased", data));
  callback(std::move(resp));
}
