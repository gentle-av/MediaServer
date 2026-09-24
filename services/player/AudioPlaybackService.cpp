#include "AudioPlaybackService.h"
#include <mutex>

AudioPlaybackService::AudioPlaybackService(
    std::unique_ptr<MpvIpcClient> ipcClient)
    : ipcClient(std::move(ipcClient)) {}

void AudioPlaybackService::setPlaylist(std::vector<std::string> tracks) {
  std::unique_lock lock(serviceMutex);
  currentTracks = std::move(tracks);
  currentIndex = -1;
  isPlaying = false;
  if (currentTracks.empty()) {
    return;
  }
  ipcClient->startMpvIfNeeded();
  ipcClient->sendCommand(R"({"command": ["playlist-clear"]})");
  nlohmann::json cmd;
  cmd["command"] = nlohmann::json::array({"loadlist"});
  cmd["command"].push_back(""); // плейсхолдер, перезапишем ниже
  cmd["command"].erase(cmd["command"].begin() + 1);
  for (const auto &track : currentTracks) {
    nlohmann::json item;
    item["command"] = nlohmann::json::array({"loadfile", track, "append"});
    ipcClient->sendCommand(item.dump());
  }
  nlohmann::json playIndex;
  playIndex["command"] = nlohmann::json::array({"playlist-play-index", 0});
  ipcClient->sendCommand(playIndex.dump());
  currentIndex = 0;
  isPlaying = true;
}

void AudioPlaybackService::loadTrackByIndex(int index) {
  std::unique_lock lock(serviceMutex);
  if (index < 0 || index >= static_cast<int>(currentTracks.size())) {
    return;
  }
  currentIndex = index;
  isPlaying = true;
  ipcClient->startMpvIfNeeded();
  nlohmann::json cmd;
  cmd["command"] = nlohmann::json::array({"playlist-play-index", index});
  ipcClient->sendCommand(cmd.dump());
}

void AudioPlaybackService::togglePause(bool pause) {
  std::unique_lock lock(serviceMutex);
  ipcClient->startMpvIfNeeded();
  nlohmann::json cmd;
  cmd["command"] = nlohmann::json::array({"set_property", "pause", pause});
  ipcClient->sendCommand(cmd.dump());
  isPlaying = !pause;
}

void AudioPlaybackService::stop() {
  std::unique_lock lock(serviceMutex);
  ipcClient->sendCommand(R"({"command": ["stop"]})");
  currentIndex = -1;
  isPlaying = false;
}

nlohmann::json AudioPlaybackService::getPlaybackState() const {
  std::shared_lock lock(serviceMutex);
  nlohmann::json state;
  if (currentTracks.empty()) {
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex;
    state["totalTracks"] = 0;
    state["currentTime"] = 0;
    state["duration"] = 0;
    return state;
  }
  std::string pauseResp =
      ipcClient->sendCommand(R"({"command": ["get_property", "pause"]})");
  bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
  double currentTime = ipcClient->parseResponseValue(
      ipcClient->sendCommand(R"({"command": ["get_property", "time-pos"]})"));
  double duration = ipcClient->parseResponseValue(
      ipcClient->sendCommand(R"({"command": ["get_property", "duration"]})"));
  state["isPlaying"] = !isPaused && (currentTime > 0 || duration > 0);
  state["currentTrack"] =
      (currentIndex >= 0 &&
       currentIndex < static_cast<int>(currentTracks.size()))
          ? currentTracks[currentIndex]
          : "";
  state["currentIndex"] = currentIndex;
  state["totalTracks"] = static_cast<int>(currentTracks.size());
  state["currentTime"] = currentTime;
  state["duration"] = duration > 0 ? duration : 0;
  state["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  return state;
}

nlohmann::json AudioPlaybackService::getTimeInfo() const {
  std::shared_lock lock(serviceMutex);
  nlohmann::json data;
  double currentTime = ipcClient->parseResponseValue(
      ipcClient->sendCommand(R"({"command": ["get_property", "time-pos"]})"));
  double duration = ipcClient->parseResponseValue(
      ipcClient->sendCommand(R"({"command": ["get_property", "duration"]})"));
  data["currentTime"] = currentTime;
  data["duration"] = duration > 0 ? duration : 0;
  data["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  return data;
}

void AudioPlaybackService::seek(double seconds) {
  std::unique_lock lock(serviceMutex);
  ipcClient->startMpvIfNeeded();
  nlohmann::json cmd;
  cmd["command"] = nlohmann::json::array({"seek", seconds, "absolute"});
  ipcClient->sendCommand(cmd.dump());
}
