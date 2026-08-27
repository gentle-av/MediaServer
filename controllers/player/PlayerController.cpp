#include "PlayerController.h"
#include "../../services/system/AlsaMixer.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

PlayerController::PlayerController(App &app, MusicRepository &musicRepo,
                                   PlaylistRepository &playlistRepo,
                                   std::shared_ptr<MetadataCache> cache)
    : RestController<App>(app), musicRepository(musicRepo),
      playlistRepository(playlistRepo), metadataCache(cache) {
  outputSwitcher = std::make_unique<AudioOutputSwitcher>();
  mpvHandle = nullptr;
  musicRepository.subscribe("track_removed", [this](
                                                 const std::string &filePath) {
    std::lock_guard<std::shared_mutex> lock(stateMutex);
    auto it = std::find(currentPlaylistTracks.begin(),
                        currentPlaylistTracks.end(), filePath);
    if (it != currentPlaylistTracks.end()) {
      int removedIndex =
          static_cast<int>(std::distance(currentPlaylistTracks.begin(), it));
      currentPlaylistTracks.erase(it);
      if (currentIndex >= removedIndex && currentIndex > 0) {
        currentIndex--;
      }
    }
  });
}

PlayerController::~PlayerController() {
  stopAutoAdvance = true;
  if (autoAdvanceTracker) {
    autoAdvanceTracker->stop();
  }
  if (idleTimerThread && idleTimerThread->joinable()) {
    idleTimerThread->join();
  }
  if (!socketPath.empty()) {
    sendMpvCommand(R"({"command": ["stop"]})");
    close(::socket(AF_UNIX, SOCK_STREAM, 0));
  }
}

void PlayerController::register_all_routes() {
  app_.post("/api/audio/play",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handlePlay(req);
            });
  app_.post("/api/audio/pause",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handlePause(req);
            });
  app_.post("/api/audio/stop",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleStop(req);
            });
  app_.post("/api/audio/next",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleNext(req);
            });
  app_.post("/api/audio/previous",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handlePrevious(req);
            });
  app_.post("/api/audio/playlist",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSetPlaylist(req);
            });
  app_.get("/api/audio/playlist/load",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleLoadPlaylist(req);
           });
  app_.post("/api/audio/file",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handlePlayFile(req);
            });
  app_.post("/api/audio/index",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handlePlayIndex(req);
            });
  app_.post("/api/audio/seek",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSeek(req);
            });
  app_.post("/api/audio/speed",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSetSpeed(req);
            });
  app_.get("/api/audio/state",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetPlaybackState(req);
           });
  app_.get("/api/audio/time",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetCurrentTime(req);
           });
  app_.get("/api/audio/volume",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetVolume(req);
           });
  app_.post("/api/audio/volume",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSetVolume(req);
            });
  app_.post("/api/audio/volume/increase",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleIncreaseVolume(req);
            });
  app_.post("/api/audio/volume/decrease",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleDecreaseVolume(req);
            });
  app_.post("/api/audio/mute",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleToggleMute(req);
            });
  app_.post("/api/audio/output/speakers",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSwitchToSpeakers(req);
            });
  app_.post("/api/audio/output/headphones",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSwitchToHeadphones(req);
            });
  app_.get("/api/audio/output",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetAudioOutput(req);
           });
  app_.post("/api/audio/force-stop",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleForceStop(req);
            });
}

std::string PlayerController::sendMpvCommand(const std::string &command) const {
  if (socketPath.empty()) {
    return "";
  }
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    return "";
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return "";
  }
  std::string cmd = command + "\n";
  send(sock, cmd.c_str(), cmd.length(), 0);
  char buffer[4096] = {0};
  int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
  close(sock);
  if (bytesRead > 0) {
    return std::string(buffer, bytesRead);
  }
  return "";
}

double PlayerController::parseMpvResponse(const std::string &response) const {
  size_t pos = response.find("\"data\"");
  if (pos == std::string::npos)
    return 0;
  size_t start = response.find(":", pos);
  if (start == std::string::npos)
    return 0;
  try {
    std::string numStr = response.substr(start + 1);
    size_t end = numStr.find_first_of(",}\n\r");
    if (end != std::string::npos) {
      numStr = numStr.substr(0, end);
    }
    return std::stod(numStr);
  } catch (...) {
    return 0;
  }
}

void PlayerController::startMpvIfNeeded() {
  if (!socketPath.empty()) {
    std::string response =
        sendMpvCommand(R"({"command": ["get_property", "pause"]})");
    if (!response.empty()) {
      return;
    }
  }
  socketPath = "/tmp/mpv-socket-" + std::to_string(getpid());
  std::string cmd =
      "mpv --input-ipc-server=" + socketPath + " --no-video --really-quiet &";
  system(cmd.c_str());
  for (int i = 0; i < 50; ++i) {
    usleep(100000);
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock >= 0) {
      struct sockaddr_un addr;
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
      if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        close(sock);
        resetIdleTimer();
        return;
      }
      close(sock);
    }
  }
}

void PlayerController::loadTrack(int index) {
  if (index < 0 || index >= static_cast<int>(currentPlaylistTracks.size())) {
    return;
  }
  std::lock_guard<std::shared_mutex> lock(stateMutex);
  currentIndex = index;
  isPlaying = true;
  std::string trackPath = currentPlaylistTracks[index];
  sendMpvCommand(R"({"command": ["loadfile", ")" + trackPath + R"("]})");
  resetIdleTimer();
}

void PlayerController::loadPlaylistInternal(const std::string &name) {
  auto playlist = playlistRepository.loadPlaylist(name);
  if (!playlist) {
    return;
  }
  std::lock_guard<std::shared_mutex> lock(stateMutex);
  currentPlaylistName = name;
  currentPlaylistTracks.clear();
  currentPlaylistTracks.reserve(playlist->size());
  for (const auto &track : playlist->getAllTracks()) {
    currentPlaylistTracks.push_back(track.filePath);
  }
  if (!currentPlaylistTracks.empty()) {
    startMpvIfNeeded();
    loadTrack(0);
  }
}

void PlayerController::resetIdleTimer() {}

bool PlayerController::ensurePlaylistLoaded() {
  if (currentPlaylistTracks.empty()) {
    return false;
  }
  startMpvIfNeeded();
  if (currentIndex < 0 ||
      currentIndex >= static_cast<int>(currentPlaylistTracks.size())) {
    loadTrack(0);
  }
  return true;
}

std::string
PlayerController::getQueryParam(const StringHttpRequest &req,
                                const std::string &key,
                                const std::string &defaultValue) const {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

int PlayerController::getQueryParamInt(const StringHttpRequest &req,
                                       const std::string &key,
                                       int defaultValue) const {
  auto value = req.getQuery(key);
  if (value.empty()) {
    return defaultValue;
  }
  try {
    return std::stoi(value);
  } catch (...) {
    return defaultValue;
  }
}

bool PlayerController::getQueryParamBool(const StringHttpRequest &req,
                                         const std::string &key,
                                         bool defaultValue) const {
  auto value = req.getQuery(key);
  if (value.empty()) {
    return defaultValue;
  }
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

nlohmann::json
PlayerController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}

std::string PlayerController::formatTime(double seconds) const {
  if (seconds < 0) {
    return "00:00";
  }
  int totalSeconds = static_cast<int>(seconds);
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int secs = totalSeconds % 60;
  std::ostringstream oss;
  if (hours > 0) {
    oss << std::setw(2) << std::setfill('0') << hours << ":";
  }
  oss << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(2)
      << std::setfill('0') << secs;
  return oss.str();
}

nlohmann::json PlayerController::trackToJson(const MusicMetadata &track) const {
  nlohmann::json obj;
  obj["path"] = track.filePath;
  obj["title"] = track.title.empty() ? "Unknown" : track.title;
  obj["artist"] = track.artist;
  obj["album"] = track.album;
  obj["duration"] = track.duration;
  obj["durationFormatted"] = formatTime(track.duration);
  obj["trackNumber"] = track.track;
  obj["year"] = track.year;
  obj["genre"] = track.genre;
  return obj;
}

nlohmann::json PlayerController::getCurrentPlaybackState() const {
  nlohmann::json state;
  if (socketPath.empty() || currentPlaylistTracks.empty()) {
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex.load();
    state["totalTracks"] = static_cast<int>(currentPlaylistTracks.size());
    state["currentTime"] = 0;
    state["duration"] = 0;
    state["playlistName"] = currentPlaylistName;
    state["hasPlaylist"] = !currentPlaylistName.empty();
    return state;
  }
  std::string pauseResp =
      sendMpvCommand(R"({"command": ["get_property", "pause"]})");
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  double currentTime = parseMpvResponse(
      sendMpvCommand(R"({"command": ["get_property", "time-pos"]})"));
  double duration = parseMpvResponse(
      sendMpvCommand(R"({"command": ["get_property", "duration"]})"));
  state["isPlaying"] = !isPaused && (currentTime > 0 || duration > 0);
  state["currentTrack"] =
      (currentIndex >= 0 &&
       currentIndex < static_cast<int>(currentPlaylistTracks.size()))
          ? currentPlaylistTracks[currentIndex]
          : "";
  state["currentIndex"] = currentIndex.load();
  state["totalTracks"] = static_cast<int>(currentPlaylistTracks.size());
  state["currentTime"] = currentTime;
  state["currentTimeFormatted"] = formatTime(currentTime);
  state["duration"] = duration > 0 ? duration : 0;
  state["durationFormatted"] = formatTime(duration);
  state["playlistName"] = currentPlaylistName;
  state["hasPlaylist"] = !currentPlaylistName.empty();
  state["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  return state;
}

nlohmann::json PlayerController::getCurrentTimeInfo() const {
  nlohmann::json data;
  if (socketPath.empty()) {
    data["currentTime"] = 0;
    data["duration"] = 0;
    data["progress"] = 0;
    return data;
  }
  double currentTime = parseMpvResponse(
      sendMpvCommand(R"({"command": ["get_property", "time-pos"]})"));
  double duration = parseMpvResponse(
      sendMpvCommand(R"({"command": ["get_property", "duration"]})"));
  data["currentTime"] = currentTime;
  data["currentTimeFormatted"] = formatTime(currentTime);
  data["duration"] = duration > 0 ? duration : 0;
  data["durationFormatted"] = formatTime(duration);
  data["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  return data;
}

StringHttpResponse PlayerController::handlePlay(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (!ensurePlaylistLoaded()) {
      res.setJsonContent(error_response(400, "No tracks in playlist").dump());
      res.setStatus(400);
      return res;
    }
    startMpvIfNeeded();
    sendMpvCommand(R"({"command": ["set_property", "pause", false]})");
    isPlaying = true;
    resetIdleTimer();
    res.setJsonContent(success_response("Playback started").dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse PlayerController::handlePause(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (socketPath.empty()) {
      res.setJsonContent(error_response(400, "Player not running").dump());
      res.setStatus(400);
      return res;
    }
    sendMpvCommand(R"({"command": ["set_property", "pause", true]})");
    isPlaying = false;
    resetIdleTimer();
    res.setJsonContent(success_response("Playback paused").dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse PlayerController::handleStop(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (!socketPath.empty()) {
      sendMpvCommand(R"({"command": ["stop"]})");
    }
    std::lock_guard<std::shared_mutex> lock(stateMutex);
    currentIndex = -1;
    isPlaying = false;
    resetIdleTimer();
    res.setJsonContent(success_response("Stopped").dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse PlayerController::handleNext(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    startMpvIfNeeded();
    int nextIndex = currentIndex + 1;
    if (nextIndex < static_cast<int>(currentPlaylistTracks.size())) {
      loadTrack(nextIndex);
    } else {
      res.setJsonContent(error_response(400, "Already at last track").dump());
      res.setStatus(400);
      return res;
    }
    resetIdleTimer();
    nlohmann::json data;
    data["index"] = nextIndex;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handlePrevious(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    startMpvIfNeeded();
    int prevIndex = currentIndex - 1;
    if (prevIndex >= 0) {
      loadTrack(prevIndex);
    } else {
      res.setJsonContent(error_response(400, "Already at first track").dump());
      res.setStatus(400);
      return res;
    }
    resetIdleTimer();
    nlohmann::json data;
    data["index"] = prevIndex;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleSetPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("tracks") || !json["tracks"].is_array()) {
      res.setJsonContent(
          error_response(400, "Missing tracks array parameter").dump());
      res.setStatus(400);
      return res;
    }
    std::vector<std::string> tracks;
    for (const auto &track : json["tracks"]) {
      if (track.is_string()) {
        std::string decoded =
            StringHttpRequest::urlDecode(track.get<std::string>());
        tracks.push_back(decoded);
      }
    }
    std::lock_guard<std::shared_mutex> lock(stateMutex);
    currentPlaylistName.clear();
    currentPlaylistTracks = std::move(tracks);
    if (!currentPlaylistTracks.empty()) {
      startMpvIfNeeded();
      loadTrack(0);
    }
    resetIdleTimer();
    nlohmann::json data;
    data["count"] = static_cast<int>(currentPlaylistTracks.size());
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleLoadPlaylist(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = getQueryParam(req, "name");
    if (name.empty()) {
      res.setJsonContent(error_response(400, "Missing name parameter").dump());
      res.setStatus(400);
      return res;
    }
    if (!playlistRepository.playlistExists(name)) {
      res.setJsonContent(error_response(404, "Playlist not found").dump());
      res.setStatus(404);
      return res;
    }
    loadPlaylistInternal(name);
    nlohmann::json data;
    data["name"] = name;
    data["count"] = static_cast<int>(currentPlaylistTracks.size());
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handlePlayFile(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("path") || !json["path"].is_string()) {
      res.setJsonContent(error_response(400, "Missing path parameter").dump());
      res.setStatus(400);
      return res;
    }
    std::string path = json["path"].get<std::string>();
    std::lock_guard<std::shared_mutex> lock(stateMutex);
    currentPlaylistName.clear();
    currentPlaylistTracks.clear();
    currentPlaylistTracks.push_back(path);
    startMpvIfNeeded();
    loadTrack(0);
    resetIdleTimer();
    nlohmann::json data;
    data["path"] = path;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handlePlayIndex(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("index") || !json["index"].is_number_integer()) {
      res.setJsonContent(error_response(400, "Missing index parameter").dump());
      res.setStatus(400);
      return res;
    }
    int index = json["index"].get<int>();
    if (index < 0 || index >= static_cast<int>(currentPlaylistTracks.size())) {
      res.setJsonContent(error_response(400, "Index out of range").dump());
      res.setStatus(400);
      return res;
    }
    startMpvIfNeeded();
    loadTrack(index);
    resetIdleTimer();
    nlohmann::json data;
    data["index"] = index;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse PlayerController::handleSeek(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("position") || !json["position"].is_number()) {
      res.setJsonContent(
          error_response(400, "Missing position parameter").dump());
      res.setStatus(400);
      return res;
    }
    double position = std::max(0.0, json["position"].get<double>());
    if (std::isnan(position) || std::isinf(position)) {
      res.setJsonContent(
          error_response(400, "Invalid position parameter").dump());
      res.setStatus(400);
      return res;
    }
    startMpvIfNeeded();
    sendMpvCommand(R"({"command": ["seek", )" + std::to_string(position) +
                   R"(, "absolute"]})");
    resetIdleTimer();
    res.setJsonContent(success_response("Seek completed").dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleSetSpeed(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("speed") || !json["speed"].is_number()) {
      res.setJsonContent(error_response(400, "Missing speed parameter").dump());
      res.setStatus(400);
      return res;
    }
    double speed = json["speed"].get<double>();
    if (speed < 0.1 || speed > 3.0) {
      res.setJsonContent(
          error_response(400, "Speed must be between 0.1 and 3.0").dump());
      res.setStatus(400);
      return res;
    }
    startMpvIfNeeded();
    sendMpvCommand(R"({"command": ["set_property", "speed", )" +
                   std::to_string(speed) + R"(]})");
    resetIdleTimer();
    nlohmann::json data;
    data["speed"] = speed;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleGetVolume(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    int volume = AlsaMixer::getInstance().getVolume();
    if (volume < 0) {
      res.setJsonContent(error_response(500, "Failed to get volume").dump());
      res.setStatus(500);
      return res;
    }
    nlohmann::json data;
    data["volume"] = volume;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleSetVolume(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      res.setJsonContent(error_response(400, "Invalid JSON body").dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("volume") || !json["volume"].is_number_integer()) {
      res.setJsonContent(
          error_response(400, "Missing volume parameter").dump());
      res.setStatus(400);
      return res;
    }
    int volume = json["volume"].get<int>();
    if (volume < 0 || volume > 100) {
      res.setJsonContent(error_response(400, "Volume must be 0-100").dump());
      res.setStatus(400);
      return res;
    }
    AlsaMixer::getInstance().setVolume(volume);
    nlohmann::json data;
    data["volume"] = volume;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleIncreaseVolume(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    AlsaMixer::getInstance().increaseVolume(5);
    nlohmann::json data;
    data["volume"] = AlsaMixer::getInstance().getVolume();
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleDecreaseVolume(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    AlsaMixer::getInstance().decreaseVolume(5);
    nlohmann::json data;
    data["volume"] = AlsaMixer::getInstance().getVolume();
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleToggleMute(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    AlsaMixer::getInstance().toggleMute();
    nlohmann::json data;
    data["muted"] = AlsaMixer::getInstance().isMuted();
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleSwitchToSpeakers(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (outputSwitcher->switchToSpeakers()) {
      nlohmann::json data;
      data["output"] = "speakers";
      res.setJsonContent(success_with_data(data).dump());
      res.setStatus(200);
    } else {
      res.setJsonContent(
          error_response(500, "Failed to switch to speakers").dump());
      res.setStatus(500);
    }
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleSwitchToHeadphones(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (outputSwitcher->switchToHeadphones()) {
      nlohmann::json data;
      data["output"] = "headphones";
      res.setJsonContent(success_with_data(data).dump());
      res.setStatus(200);
    } else {
      res.setJsonContent(
          error_response(500, "Failed to switch to headphones").dump());
      res.setStatus(500);
    }
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleGetAudioOutput(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json data;
    data["current"] = outputSwitcher->getCurrentOutput();
    nlohmann::json available = nlohmann::json::array();
    for (const auto &output : outputSwitcher->getAvailableOutputs()) {
      available.push_back(output);
    }
    data["available"] = available;
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleForceStop(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    stopAutoAdvance = true;
    if (autoAdvanceTracker) {
      autoAdvanceTracker->stop();
    }
    if (idleTimerThread && idleTimerThread->joinable()) {
      idleTimerThread->join();
    }
    if (!socketPath.empty()) {
      sendMpvCommand(R"({"command": ["stop"]})");
      close(::socket(AF_UNIX, SOCK_STREAM, 0));
      socketPath.clear();
    }
    currentPlaylistTracks.clear();
    currentIndex = -1;
    isPlaying = false;
    res.setJsonContent(success_response("Audio force stopped").dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleGetPlaybackState(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json state = getCurrentPlaybackState();
    res.setJsonContent(success_with_data(state).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
PlayerController::handleGetCurrentTime(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    nlohmann::json data = getCurrentTimeInfo();
    res.setJsonContent(success_with_data(data).dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setJsonContent(error_response(500, e.what()).dump());
    res.setStatus(500);
  }
  return res;
}
