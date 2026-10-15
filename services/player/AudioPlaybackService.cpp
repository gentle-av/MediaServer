#include "AudioPlaybackService.h"
#include <mutex>

AudioPlaybackService::AudioPlaybackService(
    std::unique_ptr<MpvIpcClient> ipcClient)
    : ipcClient(std::move(ipcClient)) {
  pollerThread = std::jthread(
      [this](std::stop_token stopToken) { this->pollerLoop(stopToken); });
}

AudioPlaybackService::~AudioPlaybackService() {
  if (pollerThread.joinable()) {
    pollerThread.request_stop();
    pollerThread.join();
  }
}

void AudioPlaybackService::pollerLoop(std::stop_token stopToken) {
  while (!stopToken.stop_requested()) {
    if (!pollerEnabled.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }
    nlohmann::json snapshot;
    {
      std::shared_lock lock(serviceMutex);
      if (currentTracks.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }
      snapshot["currentIndex"] = currentIndex;
      snapshot["totalTracks"] = static_cast<int>(currentTracks.size());
      snapshot["currentTrack"] =
          (currentIndex >= 0 &&
           currentIndex < static_cast<int>(currentTracks.size()))
              ? currentTracks[currentIndex]
              : "";
      snapshot["isPlaying"] = isPlaying;
    }
    std::string pauseResp =
        ipcClient->sendCommand(R"({"command": ["get_property", "pause"]})");
    bool paused = pauseResp.find("\"data\":true") != std::string::npos;
    double currentTime = ipcClient->parseResponseValue(
        ipcClient->sendCommand(R"({"command": ["get_property", "time-pos"]})"));
    double duration = ipcClient->parseResponseValue(
        ipcClient->sendCommand(R"({"command": ["get_property", "duration"]})"));
    snapshot["currentTime"] = currentTime;
    snapshot["duration"] = duration > 0 ? duration : 0.0;
    snapshot["progress"] = duration > 0 ? (currentTime / duration * 100) : 0.0;
    snapshot["isPlaying"] = snapshot.value("isPlaying", false) && !paused;
    snapshot["paused"] = paused;
    {
      std::unique_lock cacheLock(cacheMutex);
      cachedState = snapshot;
      cachedStateTime = std::chrono::steady_clock::now();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void AudioPlaybackService::setPlaylist(std::vector<std::string> tracks,
                                       int initialIndex) {
  {
    std::unique_lock lock(serviceMutex);
    currentTracks = std::move(tracks);
    currentIndex = -1;
    isPlaying = false;
    if (currentTracks.empty()) {
      pollerEnabled.store(false);
      return;
    }
  }
  std::thread([this, initialIndex]() {
    std::vector<std::string> localTracks;
    {
      std::shared_lock lock(serviceMutex);
      localTracks = currentTracks;
    }
    if (localTracks.empty()) {
      return;
    }
    ipcClient->startMpvIfNeeded();
    ipcClient->sendCommand(R"({"command": ["playlist-clear"]})");
    for (size_t i = 0; i < localTracks.size(); ++i) {
      nlohmann::json item;
      if (i == 0) {
        item["command"] =
            nlohmann::json::array({"loadfile", localTracks[i], "replace"});
      } else {
        item["command"] =
            nlohmann::json::array({"loadfile", localTracks[i], "append"});
      }
      ipcClient->sendCommand(item.dump());
    }
    if (initialIndex >= 0 &&
        initialIndex < static_cast<int>(localTracks.size())) {
      std::unique_lock lock(serviceMutex);
      currentIndex = initialIndex;
      isPlaying = true;
      if (initialIndex > 0) {
        nlohmann::json playCmd;
        playCmd["command"] =
            nlohmann::json::array({"playlist-play-index", initialIndex});
        ipcClient->sendCommand(playCmd.dump());
      }
    }
    pollerEnabled.store(true);
  }).detach();
}

void AudioPlaybackService::loadTrackByIndex(int index) {
  {
    std::unique_lock lock(serviceMutex);
    if (index < 0 || index >= static_cast<int>(currentTracks.size())) {
      return;
    }
    currentIndex = index;
    isPlaying = true;
  }
  std::thread([this, index]() {
    ipcClient->startMpvIfNeeded();
    nlohmann::json cmd;
    cmd["command"] = nlohmann::json::array({"playlist-play-index", index});
    ipcClient->sendCommand(cmd.dump());
    pollerEnabled.store(true);
  }).detach();
}

void AudioPlaybackService::togglePause(bool pause) {
  {
    std::unique_lock lock(serviceMutex);
    isPlaying = !pause;
  }
  std::thread([this, pause]() {
    ipcClient->startMpvIfNeeded();
    nlohmann::json cmd;
    cmd["command"] = nlohmann::json::array({"set_property", "pause", pause});
    ipcClient->sendCommand(cmd.dump());
  }).detach();
}

void AudioPlaybackService::stop() {
  {
    std::unique_lock lock(serviceMutex);
    currentIndex = -1;
    isPlaying = false;
  }
  pollerEnabled.store(false);
  std::thread([this]() {
    ipcClient->sendCommand(R"({"command": ["stop"]})");
  }).detach();
}

void AudioPlaybackService::seek(double seconds) {
  std::thread([this, seconds]() {
    ipcClient->startMpvIfNeeded();
    nlohmann::json cmd;
    cmd["command"] = nlohmann::json::array({"seek", seconds, "absolute"});
    ipcClient->sendCommand(cmd.dump());
  }).detach();
}

nlohmann::json AudioPlaybackService::getPlaybackState() const {
  std::shared_lock cacheLock(cacheMutex);
  if (cachedState.is_null() || cachedState.empty()) {
    nlohmann::json state;
    std::shared_lock lock(serviceMutex);
    state["isPlaying"] = false;
    state["currentTrack"] = "";
    state["currentIndex"] = currentIndex;
    state["totalTracks"] = static_cast<int>(currentTracks.size());
    state["currentTime"] = 0.0;
    state["duration"] = 0.0;
    state["progress"] = 0.0;
    return state;
  }
  return cachedState;
}

nlohmann::json AudioPlaybackService::getTimeInfo() const {
  std::shared_lock cacheLock(cacheMutex);
  nlohmann::json data;
  if (cachedState.is_null() || cachedState.empty()) {
    data["currentTime"] = 0.0;
    data["duration"] = 0.0;
    data["progress"] = 0.0;
    return data;
  }
  data["currentTime"] = cachedState.value("currentTime", 0.0);
  data["duration"] = cachedState.value("duration", 0.0);
  data["progress"] = cachedState.value("progress", 0.0);
  return data;
}
