#include "SimplePlayerController.h"
#include <chrono>
#include <thread>
#include <unistd.h>

void SimplePlayerController::handleGetVolume(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string cmd =
      "amixer get Master 2>/dev/null | grep -oP '\\d+(?=%)' | head -1";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  int volume = -1;
  if (pipe) {
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      volume = std::stoi(std::string(buffer.data()));
    }
    pclose(pipe);
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

void SimplePlayerController::handleSetVolume(
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
    std::string cmd =
        "amixer set Master " + std::to_string(volume) + "% 2>/dev/null";
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

void SimplePlayerController::handleIncreaseVolume(
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
    // Получаем текущий процент
    std::string getCmd =
        "amixer get Master 2>/dev/null | grep -oP '\\d+(?=%)' | head -1";
    std::array<char, 128> buffer;
    FILE *pipe = popen(getCmd.c_str(), "r");
    int currentPercent = 0;
    if (pipe && fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      currentPercent = std::stoi(std::string(buffer.data()));
    }
    if (pipe)
      pclose(pipe);

    int newPercent = std::min(100, currentPercent + delta);
    // Устанавливаем новый процент
    std::string cmd =
        "amixer set Master " + std::to_string(newPercent) + "% 2>/dev/null";
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

void SimplePlayerController::handleDecreaseVolume(
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
    // Получаем текущий процент
    std::string getCmd =
        "amixer get Master 2>/dev/null | grep -oP '\\d+(?=%)' | head -1";
    std::array<char, 128> buffer;
    FILE *pipe = popen(getCmd.c_str(), "r");
    int currentPercent = 0;
    if (pipe && fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      currentPercent = std::stoi(std::string(buffer.data()));
    }
    if (pipe)
      pclose(pipe);

    int newPercent = std::max(0, currentPercent - delta);
    // Устанавливаем новый процент
    std::string cmd =
        "amixer set Master " + std::to_string(newPercent) + "% 2>/dev/null";
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

void SimplePlayerController::handleToggleMute(
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
  // Получаем состояние mute
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

void SimplePlayerController::launchMpv() {
  unlink(socketPath_.c_str());
  std::string cmd = "mpv --input-ipc-server=" + socketPath_ +
                    " --idle --no-video --ao=alsa" +
                    " --audio-device=alsa/front:CARD=II,DEV=0" +
                    " --no-terminal" + " > /dev/null 2>&1 &";
  system(cmd.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::string SimplePlayerController::sendCommand(const std::string &jsonCmd) {
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

bool SimplePlayerController::isProcessAlive() {
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

void SimplePlayerController::handleNewSetPlaylist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "\n=== handleNewSetPlaylist START ===" << std::endl;
  try {
    Json::Value json = parseBody(req);
    std::cout << "Parsed JSON: " << json.toStyledString() << std::endl;
    if (!json.isMember("tracks") || !json["tracks"].isArray()) {
      std::cout << "ERROR: Missing tracks array" << std::endl;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing tracks array parameter"));
      callback(resp);
      return;
    }
    playlist_.clear();
    for (const auto &track : json["tracks"]) {
      if (track.isString()) {
        std::string path = track.asString();
        std::cout << "Adding track: " << path << std::endl;
        playlist_.push_back(path);
      }
    }
    if (!playlist_.empty()) {
      currentIndex_ = 0;
      std::cout << "Playing track at index 0: " << playlist_[0] << std::endl;
      std::string cmd =
          R"({"command": ["loadfile", ")" + playlist_[0] + R"(", "replace"]})";
      std::cout << "Sending loadfile command" << std::endl;
      std::string response = sendCommand(cmd);
      std::cout << "Loadfile response: " << response << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::cout << "Sending play command" << std::endl;
      sendCommand(R"({"command": ["set_property", "pause", false]})");
    } else {
      currentIndex_ = -1;
    }
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Playlist set"));
    callback(resp);
  } catch (const std::exception &e) {
    std::cout << "EXCEPTION: " << e.what() << std::endl;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}

void SimplePlayerController::handleNewPlay(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "=== handleNewPlay ===" << std::endl;
  std::string response =
      sendCommand(R"({"command": ["set_property", "pause", false]})");
  std::cout << "Play response: " << response << std::endl;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started"));
  callback(resp);
}

void SimplePlayerController::handleNewGetPlaybackState(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "=== handleNewGetPlaybackState ===" << std::endl;
  Json::Value state;
  if (!isProcessAlive()) {
    std::cout << "Process not alive" << std::endl;
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex_;
    state["totalTracks"] = (int)playlist_.size();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "", state));
    callback(resp);
    return;
  }
  std::string pauseResp =
      sendCommand(R"({"command": ["get_property", "pause"]})");
  std::cout << "Pause response raw: " << pauseResp << std::endl;
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  std::cout << "Is paused: " << isPaused << std::endl;
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  std::cout << "Time response raw: " << timeResp << std::endl;
  double currentTime = 0;
  size_t pos = timeResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = timeResp.find(":", pos);
    if (start != std::string::npos) {
      currentTime = std::stod(timeResp.substr(start + 1));
      std::cout << "Current time: " << currentTime << std::endl;
    }
  }
  state["isPlaying"] = !isPaused && currentTime > 0;
  state["currentTrack"] =
      (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size())
          ? playlist_[currentIndex_]
          : "";
  state["currentIndex"] = currentIndex_;
  state["totalTracks"] = (int)playlist_.size();
  std::cout << "Final state: " << state.toStyledString() << std::endl;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", state));
  callback(resp);
}

int SimplePlayerController::instanceCounter_ = 0;

SimplePlayerController::SimplePlayerController() : currentIndex_(-1) {
  socketPath_ = "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
                std::to_string(instanceCounter_++);
  launchMpv();
  system("amixer set Master 25% 2>/dev/null");
}

SimplePlayerController::~SimplePlayerController() { killMpv(); }

void SimplePlayerController::killMpv() {
  if (!socketPath_.empty()) {
    sendCommand(R"({"command": ["quit"]})");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    socketPath_.clear();
  }
}

Json::Value
SimplePlayerController::parseBody(const drogon::HttpRequestPtr &req) {
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

Json::Value SimplePlayerController::jsonResponse(bool success,
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

void SimplePlayerController::handleNewPause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["set_property", "pause", true]})");
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused"));
  callback(resp);
}

void SimplePlayerController::handleNewStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["stop"]})");
  currentIndex_ = -1;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Stopped"));
  callback(resp);
}

void SimplePlayerController::handleNewNext(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (currentIndex_ + 1 < (int)playlist_.size()) {
    currentIndex_++;
    std::string cmd = R"({"command": ["loadfile", ")" +
                      playlist_[currentIndex_] + R"(", "replace"]})";
    sendCommand(cmd);
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Next track"));
  callback(resp);
}

void SimplePlayerController::handleNewPrevious(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  if (currentIndex_ - 1 >= 0) {
    currentIndex_--;
    std::string cmd = R"({"command": ["loadfile", ")" +
                      playlist_[currentIndex_] + R"(", "replace"]})";
    sendCommand(cmd);
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Previous track"));
  callback(resp);
}

void SimplePlayerController::handleNewAddToPlaylist(
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

void SimplePlayerController::handleNewPlayFile(
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

void SimplePlayerController::handleNewClear(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  sendCommand(R"({"command": ["stop"]})");
  playlist_.clear();
  currentIndex_ = -1;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "Cleared"));
  callback(resp);
}

void SimplePlayerController::handleNewGetPlaylist(
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

void SimplePlayerController::handleNewGetCurrentTime(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value time;
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  std::string durationResp =
      sendCommand(R"({"command": ["get_property", "duration"]})");
  double currentTime = 0, duration = 0;
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
  time["currentTime"] = currentTime;
  time["duration"] = duration;
  auto resp =
      drogon::HttpResponse::newHttpJsonResponse(jsonResponse(true, "", time));
  callback(resp);
}

void SimplePlayerController::handleNewSeek(
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
