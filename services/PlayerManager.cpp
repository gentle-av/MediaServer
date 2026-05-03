// services/PlayerManager.cpp
#include "services/PlayerManager.h"
#include "services/AlsaMixer.h"
#include <array>
#include <drogon/utils/Utilities.h>
#include <mutex>
#include <regex>
#include <unistd.h>

int PlayerManager::instanceCounter_ = 0;

std::vector<std::string> PlayerManager::getPlaylist() const {
  std::lock_guard<std::mutex> lock(playlistMutex_);
  return playlist_;
}

bool PlayerManager::isAlive() const {
  return const_cast<PlayerManager *>(this)->isProcessAlive();
}

PlayerManager::~PlayerManager() {
  stopAutoAdvance_ = true;
  if (autoAdvanceThread_ && autoAdvanceThread_->joinable()) {
    autoAdvanceThread_->join();
  }
  if (idleTimerThread_ && idleTimerThread_->joinable()) {
    idleTimerThread_->join();
  }
  stopMpv();
}

PlayerManager &PlayerManager::getInstance() {
  static PlayerManager instance;
  return instance;
}

void PlayerManager::launchMpv() {
  if (socketPath_.empty())
    return;
  unlink(socketPath_.c_str());
  std::string cmd = "mpv --input-ipc-server=" + socketPath_ +
                    " --idle --no-video --ao=alsa" +
                    " --no-terminal --really-quiet > /dev/null 2>&1 &";
  system(cmd.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void PlayerManager::stopMpv() {
  std::lock_guard<std::mutex> lock(playlistMutex_);
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

bool PlayerManager::isProcessAlive() {
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

std::string PlayerManager::sendCommand(const std::string &jsonCmd) {
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

void PlayerManager::loadTrack(int index) {
  std::lock_guard<std::mutex> lock(playlistMutex_);
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

std::string PlayerManager::escapePath(const std::string &path) {
  std::string escaped = path;
  size_t pos = 0;
  while ((pos = escaped.find("\"", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return escaped;
}

void PlayerManager::startMpvIfNeeded() {
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
          if (stopAutoAdvance_ || !isProcessAlive())
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
          if (eofReached) {
            std::lock_guard<std::mutex> lock(playlistMutex_);
            if (currentIndex_ + 1 < (int)playlist_.size()) {
              loadTrack(currentIndex_ + 1);
            }
          }
        }
      });
    }
    resetIdleTimer();
  }
}

void PlayerManager::resetIdleTimer() {
  if (!isProcessAlive())
    return;
  scheduleStop();
}

void PlayerManager::scheduleStop() {
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

void PlayerManager::play() {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["set_property", "pause", false]})");
  isPlaying_ = true;
  resetIdleTimer();
}

void PlayerManager::pause() {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["set_property", "pause", true]})");
  isPlaying_ = false;
  resetIdleTimer();
}

void PlayerManager::stop() {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["stop"]})");
  currentIndex_ = -1;
  isPlaying_ = false;
  resetIdleTimer();
}

void PlayerManager::next() {
  startMpvIfNeeded();
  std::lock_guard<std::mutex> lock(playlistMutex_);
  if (currentIndex_ + 1 < (int)playlist_.size())
    loadTrack(currentIndex_ + 1);
  resetIdleTimer();
}

void PlayerManager::previous() {
  startMpvIfNeeded();
  std::lock_guard<std::mutex> lock(playlistMutex_);
  if (currentIndex_ - 1 >= 0)
    loadTrack(currentIndex_ - 1);
  resetIdleTimer();
}

void PlayerManager::setPlaylist(const std::vector<std::string> &tracks) {
  std::lock_guard<std::mutex> lock(playlistMutex_);
  playlist_.clear();
  for (const auto &track : tracks) {
    playlist_.push_back(drogon::utils::urlDecode(track));
  }
  if (!playlist_.empty()) {
    startMpvIfNeeded();
    loadTrack(0);
  }
  resetIdleTimer();
}

void PlayerManager::addToPlaylist(const std::string &track) {
  startMpvIfNeeded();
  std::lock_guard<std::mutex> lock(playlistMutex_);
  playlist_.push_back(drogon::utils::urlDecode(track));
  resetIdleTimer();
}

void PlayerManager::clearPlaylist() {
  startMpvIfNeeded();
  sendCommand(R"({"command": ["stop"]})");
  std::lock_guard<std::mutex> lock(playlistMutex_);
  playlist_.clear();
  currentIndex_ = -1;
  isPlaying_ = false;
  resetIdleTimer();
}

void PlayerManager::playFile(const std::string &path) {
  clearPlaylist();
  addToPlaylist(path);
  play();
}

void PlayerManager::playIndex(int index) {
  startMpvIfNeeded();
  std::lock_guard<std::mutex> lock(playlistMutex_);
  if (index >= 0 && index < (int)playlist_.size()) {
    loadTrack(index);
    resetIdleTimer();
  }
}

void PlayerManager::seek(double position) {
  startMpvIfNeeded();
  position = std::max(0.0, position);
  sendCommand(R"({"command": ["seek", )" + std::to_string(position) +
              R"(, "absolute"]})");
  resetIdleTimer();
}

void PlayerManager::forceStop() { stopMpv(); }

int PlayerManager::getCurrentIndex() const { return currentIndex_; }

bool PlayerManager::isPlaying() const { return isPlaying_; }

PlayerManager::PlaybackState PlayerManager::getPlaybackState() {
  PlaybackState state;
  state.currentIndex = currentIndex_;
  std::lock_guard<std::mutex> lock(playlistMutex_);
  state.totalTracks = playlist_.size();
  state.currentTrack =
      (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size())
          ? playlist_[currentIndex_]
          : "";
  if (!isProcessAlive()) {
    state.isPlaying = false;
    state.currentTime = 0;
    state.duration = 0;
    return state;
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
  state.isPlaying = !isPaused && (currentTime > 0 || duration > 0);
  state.currentTime = currentTime;
  state.duration = duration > 0 ? duration : 300;
  return state;
}

double PlayerManager::getCurrentTime() {
  if (!isProcessAlive())
    return 0;
  std::string timeResp =
      sendCommand(R"({"command": ["get_property", "time-pos"]})");
  size_t pos = timeResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = timeResp.find(":", pos);
    if (start != std::string::npos) {
      try {
        return std::stod(timeResp.substr(start + 1));
      } catch (...) {
      }
    }
  }
  return 0;
}

void PlayerManager::setVolume(int volume) {
  if (volume < 0)
    volume = 0;
  if (volume > 100)
    volume = 100;
  int amixerValue = 135 + (volume * (255 - 135) / 100);
  system(("timeout 2 amixer set Master " + std::to_string(amixerValue) +
          " 2>/dev/null")
             .c_str());
}

int PlayerManager::getVolume() {
  std::string cmd = "timeout 2 amixer get Master 2>/dev/null";
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return 50;
  while (fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  pclose(pipe);
  std::regex volumeRegex(R"((\d+)%)");
  std::smatch match;
  if (std::regex_search(result, match, volumeRegex)) {
    return std::stoi(match[1].str());
  }
  return 50;
}

void PlayerManager::increaseVolume() {
  system("timeout 2 amixer set Master 5%+ 2>/dev/null");
}

void PlayerManager::decreaseVolume() {
  system("timeout 2 amixer set Master 5%- 2>/dev/null");
}

void PlayerManager::toggleMute() {
  system("timeout 2 amixer set Master toggle 2>/dev/null");
}

void PlayerManager::switchToSpeakers() {
  AlsaMixer::getInstance().switchToSpeakers();
}

void PlayerManager::switchToHeadphones() {
  AlsaMixer::getInstance().switchToHeadphones();
}

std::string PlayerManager::getCurrentOutput() {
  std::string output = AlsaMixer::getInstance().getCurrentOutput();
  return output.empty() ? "speakers" : output;
}
