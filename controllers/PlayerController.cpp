#include "PlayerController.h"
#include "services/AlsaMixer.h"
#include <chrono>
#include <regex>
#include <thread>
#include <unistd.h>

void PlayerController::launchMpv() {
  unlink(socketPath_.c_str());
  std::string cmd = "mpv --input-ipc-server=" + socketPath_ +
                    " --idle --no-video --ao=alsa" +
                    " --no-terminal > /dev/null 2>&1 &";
  system(cmd.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void PlayerController::handleNewSetPlaylist(
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
    sendCommand(R"({"command": ["stop"]})");
    playlist_.clear();
    for (const auto &track : json["tracks"]) {
      if (track.isString()) {
        playlist_.push_back(track.asString());
      }
    }
    if (!playlist_.empty()) {
      currentIndex_ = 0;
      std::string cmd = "{\"command\": [\"loadfile\", \"" + playlist_[0] +
                        "\", \"replace\"]}";
      sendCommand(cmd);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      sendCommand("{\"command\": [\"set_property\", \"pause\", false]}");
    } else {
      currentIndex_ = -1;
    }
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playlist set"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::startAutoAdvance() {
  std::thread([this]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      if (!isProcessAlive()) {
        continue;
      }
      if (currentIndex_ < 0 || currentIndex_ >= (int)playlist_.size()) {
        continue;
      }
      std::string timeResp =
          sendCommand("{\"command\": [\"get_property\", \"time-pos\"]}");
      std::string durationResp =
          sendCommand("{\"command\": [\"get_property\", \"duration\"]}");
      std::string eofResp =
          sendCommand("{\"command\": [\"get_property\", \"eof-reached\"]}");
      double currentTime = 0, duration = 0;
      bool eofReached = eofResp.find("\"data\":true") != std::string::npos;
      size_t pos = timeResp.find("\"data\"");
      if (pos != std::string::npos) {
        size_t start = timeResp.find(":", pos);
        if (start != std::string::npos) {
          currentTime = std::stod(timeResp.substr(start + 1));
        }
      }
      pos = durationResp.find("\"data\"");
      if (pos != std::string::npos) {
        size_t start = durationResp.find(":", pos);
        if (start != std::string::npos) {
          duration = std::stod(durationResp.substr(start + 1));
        }
      }
      if ((eofReached || (duration > 0 && (duration - currentTime) < 0.5)) &&
          currentIndex_ + 1 < (int)playlist_.size()) {
        currentIndex_++;
        std::string loadCmd = "{\"command\": [\"loadfile\", \"" +
                              playlist_[currentIndex_] + "\", \"replace\"]}";
        sendCommand(loadCmd);
        sendCommand("{\"command\": [\"set_property\", \"pause\", false]}");
      }
    }
  }).detach();
}

void PlayerController::handleGetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string cmd = "amixer get Master 2>/dev/null";
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to execute amixer"));
    callback(resp);
    return;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  std::cout << "[DEBUG] amixer output:\n" << result << std::endl;
  int volume = -1;
  std::regex volumeRegex(R"((\d+)%)");
  std::smatch match;
  if (std::regex_search(result, match, volumeRegex)) {
    volume = std::stoi(match[1].str());
    std::cout << "[DEBUG] Parsed volume: " << volume << "%" << std::endl;
  } else {
    std::regex altRegex(R"(Playback\s+\d+\s+\[(\d+)%\])");
    if (std::regex_search(result, match, altRegex)) {
      volume = std::stoi(match[1].str());
      std::cout << "[DEBUG] Parsed volume (alt): " << volume << "%"
                << std::endl;
    }
  }
  if (volume < 0) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to get volume"));
    callback(resp);
    return;
  }
  Json::Value data;
  data["volume"] = volume;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PlayerController::handleSetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("volume") || !json["volume"].isInt()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing volume parameter (0-100)"));
      callback(resp);
      return;
    }
    int volume = json["volume"].asInt();
    if (volume < 0 || volume > 100) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Volume must be between 0 and 100"));
      callback(resp);
      return;
    }
    int amixerValue = 135 + (volume * (255 - 135) / 100);
    if (amixerValue < 135)
      amixerValue = 135;
    if (amixerValue > 255)
      amixerValue = 255;
    std::string cmd =
        "amixer set Master " + std::to_string(amixerValue) + " 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret != 0) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Failed to set volume"));
      callback(resp);
      return;
    }
    Json::Value data;
    data["volume"] = volume;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonResponse(
        true, "Volume set to " + std::to_string(volume) + "%", data));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleIncreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    int delta = 5;
    if (json.isMember("delta") && json["delta"].isInt()) {
      delta = json["delta"].asInt();
      if (delta <= 0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            jsonResponse(false, "Delta must be positive"));
        callback(resp);
        return;
      }
    }
    std::string getCmd = "amixer get Master 2>/dev/null";
    std::array<char, 256> buffer;
    std::string result;
    FILE *pipe = popen(getCmd.c_str(), "r");
    if (!pipe) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Failed to get current volume"));
      callback(resp);
      return;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
    int currentPercent = 0;
    std::regex volumeRegex(R"((\d+)%)");
    std::smatch match;
    if (std::regex_search(result, match, volumeRegex)) {
      currentPercent = std::stoi(match[1].str());
    }
    int newPercent = std::min(100, currentPercent + delta);
    int amixerValue = 135 + (newPercent * (255 - 135) / 100);
    if (amixerValue < 135)
      amixerValue = 135;
    if (amixerValue > 255)
      amixerValue = 255;
    std::string cmd =
        "amixer set Master " + std::to_string(amixerValue) + " 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret != 0) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Failed to increase volume"));
      callback(resp);
      return;
    }
    Json::Value data;
    data["volume"] = newPercent;
    data["delta"] = delta;
    data["old_volume"] = currentPercent;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true,
                     "Volume increased from " + std::to_string(currentPercent) +
                         "% to " + std::to_string(newPercent) + "%",
                     data));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleDecreaseVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    int delta = 5;
    if (json.isMember("delta") && json["delta"].isInt()) {
      delta = json["delta"].asInt();
      if (delta <= 0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            jsonResponse(false, "Delta must be positive"));
        callback(resp);
        return;
      }
    }
    std::string getCmd = "amixer get Master 2>/dev/null";
    std::array<char, 256> buffer;
    std::string result;
    FILE *pipe = popen(getCmd.c_str(), "r");
    if (!pipe) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Failed to get current volume"));
      callback(resp);
      return;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
    int currentPercent = 0;
    std::regex volumeRegex(R"((\d+)%)");
    std::smatch match;
    if (std::regex_search(result, match, volumeRegex)) {
      currentPercent = std::stoi(match[1].str());
    }
    int newPercent = std::max(0, currentPercent - delta);
    int amixerValue = 135 + (newPercent * (255 - 135) / 100);
    if (amixerValue < 135)
      amixerValue = 135;
    if (amixerValue > 255)
      amixerValue = 255;
    std::string cmd =
        "amixer set Master " + std::to_string(amixerValue) + " 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret != 0) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Failed to decrease volume"));
      callback(resp);
      return;
    }
    Json::Value data;
    data["volume"] = newPercent;
    data["delta"] = delta;
    data["old_volume"] = currentPercent;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true,
                     "Volume decreased from " + std::to_string(currentPercent) +
                         "% to " + std::to_string(newPercent) + "%",
                     data));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleToggleMute(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string cmd = "amixer set Master toggle 2>/dev/null";
  int ret = system(cmd.c_str());
  if (ret != 0) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to toggle mute"));
    callback(resp);
    return;
  }
  std::string getCmd = "amixer get Master 2>/dev/null | grep -oP "
                       "'(?<=\\[)(on|off)(?=\\])' | head -1";
  std::array<char, 128> buffer;
  FILE *pipe = popen(getCmd.c_str(), "r");
  std::string muted;
  if (pipe && fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    muted = std::string(buffer.data());
    muted.erase(muted.find_last_not_of(" \n\r\t") + 1);
  }
  if (pipe)
    pclose(pipe);
  Json::Value data;
  bool isMuted = (muted == "off");
  data["muted"] = isMuted;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, isMuted ? "Muted" : "Unmuted", data));
  callback(resp);
}

std::string PlayerController::sendCommand(const std::string &jsonCmd) {
  if (socketPath_.empty()) {
    return "";
  }
  if (!isProcessAlive()) {
    launchMpv();
  }
  std::string cmd = "echo '" + jsonCmd + "' | socat - " + socketPath_ + " 2>&1";
  std::array<char, 512> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
  }
  return result;
}

bool PlayerController::isProcessAlive() {
  if (socketPath_.empty())
    return false;
  if (access(socketPath_.c_str(), F_OK) != 0)
    return false;
  std::string cmd = "pgrep -f 'mpv.*" + socketPath_ + "'";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
  }
  return !result.empty();
}

void PlayerController::handleNewPlay(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["set_property", "pause", false]})");
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started"));
  callback(resp);
}

void PlayerController::handleNewGetPlaybackState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value state;
  std::cout << "[DEBUG] GetPlaybackState called - currentIndex: "
            << currentIndex_ << ", playlist size: " << playlist_.size()
            << std::endl;
  if (!isProcessAlive()) {
    std::cout << "[DEBUG] Process not alive" << std::endl;
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex_;
    state["totalTracks"] = (int)playlist_.size();
    state["currentTime"] = 0;
    state["duration"] = 0;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", state));
    callback(resp);
    return;
  }
  std::string pauseResp =
      sendCommand(R"({"command": ["get_property", "pause"]})");
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  double currentTime = 0;
  size_t pos = timeResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = timeResp.find(":", pos);
    if (start != std::string::npos) {
      currentTime = std::stod(timeResp.substr(start + 1));
    }
  }
  std::string durationResp =
      sendCommand(R"({"command": ["get_property", "duration"]})");
  double duration = 0;
  pos = durationResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = durationResp.find(":", pos);
    if (start != std::string::npos) {
      duration = std::stod(durationResp.substr(start + 1));
    }
  }
  std::cout << "[DEBUG] Playback state - isPaused: " << isPaused
            << ", currentTime: " << currentTime << ", duration: " << duration
            << std::endl;
  bool isPlaying = !isPaused && currentTime > 0;
  if (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size() &&
      duration == 0 && currentTime == 0 && playlist_.size() > 0) {
    duration = 300;
  }
  if (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size() &&
      duration > 0 && (duration - currentTime) < 0.5) {
    if (currentIndex_ + 1 < (int)playlist_.size()) {
      currentIndex_++;
      std::string loadCmd = R"({"command": ["loadfile", ")" +
                            playlist_[currentIndex_] + R"(", "replace"]})";
      sendCommand(loadCmd);
      sendCommand(R"({"command": ["set_property", "pause", false]})");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      state["currentTrack"] = playlist_[currentIndex_];
      state["currentIndex"] = currentIndex_;
      state["isPlaying"] = true;
      state["totalTracks"] = (int)playlist_.size();
      state["currentTime"] = 0;
      state["duration"] = 0;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(true, "", state));
      callback(resp);
      return;
    }
  }
  state["isPlaying"] = isPlaying;
  state["currentTrack"] =
      (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size())
          ? playlist_[currentIndex_]
          : "";
  state["currentIndex"] = currentIndex_;
  state["totalTracks"] = (int)playlist_.size();
  state["currentTime"] = currentTime;
  state["duration"] = duration;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", state));
  callback(resp);
}

int PlayerController::instanceCounter_ = 0;

PlayerController::PlayerController() : currentIndex_(-1) {
  socketPath_ = "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
                std::to_string(instanceCounter_++);
  launchMpv();
  system("amixer set Master 25% 2>/dev/null");
  startAutoAdvance();
}

PlayerController::~PlayerController() { killMpv(); }

void PlayerController::killMpv() {
  if (!socketPath_.empty()) {
    sendCommand(R"({"command": ["quit"]})");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    socketPath_.clear();
  }
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

void PlayerController::handleNewPause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["set_property", "pause", true]})");
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused"));
  callback(resp);
}

void PlayerController::handleNewStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["stop"]})");
  currentIndex_ = -1;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Stopped"));
  callback(resp);
}

void PlayerController::handleNewNext(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (currentIndex_ + 1 < (int)playlist_.size()) {
    currentIndex_++;
    std::string cmd = R"({"command": ["loadfile", ")" +
                      playlist_[currentIndex_] + R"(", "replace"]})";
    sendCommand(cmd);
    sendCommand(R"({"command": ["set_property", "pause", false]})");
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Next track"));
  callback(resp);
}

void PlayerController::handleNewPrevious(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (currentIndex_ - 1 >= 0) {
    currentIndex_--;
    std::string cmd = R"({"command": ["loadfile", ")" +
                      playlist_[currentIndex_] + R"(", "replace"]})";
    sendCommand(cmd);
    sendCommand(R"({"command": ["set_property", "pause", false]})");
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Previous track"));
  callback(resp);
}

void PlayerController::handleNewAddToPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("track") || !json["track"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing track parameter"));
      callback(resp);
      return;
    }
    playlist_.push_back(json["track"].asString());
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track added"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleNewPlayFile(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("path") || !json["path"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing path parameter"));
      callback(resp);
      return;
    }
    playlist_.clear();
    playlist_.push_back(json["path"].asString());
    currentIndex_ = 0;
    std::string cmd = R"({"command": ["loadfile", ")" +
                      json["path"].asString() + R"(", "replace"]})";
    sendCommand(cmd);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playing file"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleNewClear(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["stop"]})");
  playlist_.clear();
  currentIndex_ = -1;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Cleared"));
  callback(resp);
}

void PlayerController::handleNewGetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value playlist(Json::arrayValue);
  for (const auto &track : playlist_) {
    playlist.append(track);
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "", playlist));
  callback(resp);
}

void PlayerController::handleNewGetCurrentTime(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  if (!isProcessAlive()) {
    response["success"] = true;
    response["data"]["currentTime"] = 0;
    response["data"]["duration"] = 0;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
    return;
  }
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp =
      sendCommand(R"({"command": ["get_property", "duration"]})");
  double currentTime = 0;
  double duration = 0;
  try {
    Json::Value timeJson;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (reader->parse(timeResp.c_str(), timeResp.c_str() + timeResp.size(),
                      &timeJson, &errors)) {
      if (timeJson.isMember("data") && timeJson["data"].isNumeric()) {
        currentTime = timeJson["data"].asDouble();
      }
    }
    Json::Value durationJson;
    if (reader->parse(durationResp.c_str(),
                      durationResp.c_str() + durationResp.size(), &durationJson,
                      &errors)) {
      if (durationJson.isMember("data") && durationJson["data"].isNumeric()) {
        duration = durationJson["data"].asDouble();
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to parse mpv response: " << e.what();
  }
  response["success"] = true;
  response["data"]["currentTime"] = currentTime;
  response["data"]["duration"] = duration;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  callback(resp);
}

void PlayerController::handleNewSeek(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("position")) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing position parameter"));
      callback(resp);
      return;
    }
    double position = json["position"].asDouble();
    if (position < 0)
      position = 0;
    std::string cmd = R"({"command": ["seek", )" + std::to_string(position) +
                      R"(, "absolute"]})";
    sendCommand(cmd);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Seek completed"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void PlayerController::handleSwitchToSpeakers(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (AlsaMixer::getInstance().switchToSpeakers()) {
    Json::Value data;
    data["output"] = "speakers";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Switched to speakers", data));
    callback(resp);
  } else {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to switch to speakers"));
    callback(resp);
  }
}

void PlayerController::handleSwitchToHeadphones(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (AlsaMixer::getInstance().switchToHeadphones()) {
    Json::Value data;
    data["output"] = "headphones";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Switched to headphones", data));
    callback(resp);
  } else {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, "Failed to switch to headphones"));
    callback(resp);
  }
}

void PlayerController::handleGetAudioOutput(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value data;
  data["current"] = AlsaMixer::getInstance().getCurrentOutput();
  Json::Value available(Json::arrayValue);
  for (const auto &output : AlsaMixer::getInstance().getAvailableOutputs()) {
    available.append(output);
  }
  data["available"] = available;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", data));
  callback(resp);
}

void PlayerController::handleNewPlayIndex(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("index") || !json["index"].isInt()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing index parameter"));
      callback(resp);
      return;
    }
    int index = json["index"].asInt();
    if (index < 0 || index >= (int)playlist_.size()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Index out of range"));
      callback(resp);
      return;
    }
    currentIndex_ = index;
    std::string cmd = "{\"command\": [\"loadfile\", \"" +
                      playlist_[currentIndex_] + "\", \"replace\"]}";
    sendCommand(cmd);
    sendCommand("{\"command\": [\"set_property\", \"pause\", false]}");
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playing track at index " + std::to_string(index)));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}
