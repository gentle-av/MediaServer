#include "PlaylistManager.h"
#include "services/system/AlsaMixer.h"
#include <algorithm>
#include <iostream>
#include <random>

bool PlaylistManager::playPlaylist(const Playlist &playlist) {
  auto tracks = playlist.getAllTracks();
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty playlist" << std::endl;
    return false;
  }
  currentPlaylist_ = tracks;
  currentIndex_ = 0;
  if (isShuffled_ && currentPlaylist_.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist_.begin(), currentPlaylist_.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playPlaylist(Playlist &&playlist) {
  auto tracks = playlist.getAllTracks();
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty playlist" << std::endl;
    return false;
  }
  currentPlaylist_ = std::move(tracks);
  currentIndex_ = 0;
  if (isShuffled_ && currentPlaylist_.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist_.begin(), currentPlaylist_.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playTracks(const std::vector<MusicMetadata> &tracks) {
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty track list" << std::endl;
    return false;
  }
  currentPlaylist_ = tracks;
  currentIndex_ = 0;
  if (isShuffled_ && currentPlaylist_.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist_.begin(), currentPlaylist_.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playTracks(std::vector<MusicMetadata> &&tracks) {
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty track list" << std::endl;
    return false;
  }
  currentPlaylist_ = std::move(tracks);
  currentIndex_ = 0;
  if (isShuffled_ && currentPlaylist_.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist_.begin(), currentPlaylist_.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playFilePaths(const std::vector<std::string> &paths) {
  if (paths.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty file list" << std::endl;
    return false;
  }
  currentPlaylist_.clear();
  currentPlaylist_.reserve(paths.size());
  for (const auto &path : paths) {
    MusicMetadata meta;
    meta.filePath = path;
    currentPlaylist_.push_back(meta);
  }
  currentIndex_ = 0;
  if (isShuffled_ && currentPlaylist_.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist_.begin(), currentPlaylist_.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::loadTrack(int index) {
  if (index < 0 || index >= static_cast<int>(currentPlaylist_.size())) {
    std::cerr << "[PlaylistManager] Invalid track index: " << index
              << std::endl;
    return false;
  }
  const auto &track = currentPlaylist_[index];
  currentIndex_ = index;
  Json::Value response =
      videoControl_.handleOpen(track.filePath, activeSocket_, true);
  bool success = response.get("success", false).asBool();
  if (success) {
    std::cout << "[PlaylistManager] Playing track " << index + 1 << "/"
              << currentPlaylist_.size() << ": "
              << (track.title.empty() ? track.filePath : track.title)
              << std::endl;
  } else {
    std::cerr << "[PlaylistManager] Failed to load track: " << track.filePath
              << " - " << response.get("error", "unknown error").asString()
              << std::endl;
    if (index + 1 < static_cast<int>(currentPlaylist_.size())) {
      return loadTrack(index + 1);
    }
  }
  return success;
}

void PlaylistManager::pause() {
  videoControl_.handleControl("pause", activeSocket_);
}

void PlaylistManager::resume() {
  videoControl_.handleControl("play", activeSocket_);
}

void PlaylistManager::togglePause() {
  auto status = getStatus();
  bool isPaused = status.get("paused", false).asBool();
  if (isPaused) {
    resume();
  } else {
    pause();
  }
}

void PlaylistManager::stop() {
  videoControl_.handleClose(activeSocket_);
  currentIndex_ = -1;
}

bool PlaylistManager::seek(double seconds) {
  if (activeSocket_.empty()) {
    std::cerr << "[PlaylistManager] No active video to seek" << std::endl;
    return false;
  }
  Json::Value response = videoControl_.handleSeek(seconds, activeSocket_);
  return response.get("success", false).asBool();
}

bool PlaylistManager::next() {
  if (currentPlaylist_.empty()) {
    return false;
  }
  int nextIndex = currentIndex_ + 1;
  if (nextIndex >= static_cast<int>(currentPlaylist_.size())) {
    if (isLooping_) {
      nextIndex = 0;
    } else {
      std::cout << "[PlaylistManager] End of playlist" << std::endl;
      return false;
    }
  }
  return loadTrack(nextIndex);
}

bool PlaylistManager::previous() {
  if (currentPlaylist_.empty() || currentIndex_ <= 0) {
    return false;
  }
  return loadTrack(currentIndex_ - 1);
}

bool PlaylistManager::setVolume(int percent) {
  auto &mixer = AlsaMixer::getInstance();
  return mixer.setVolume(percent);
}

int PlaylistManager::getVolume() const {
  auto &mixer = AlsaMixer::getInstance();
  return mixer.getVolume();
}

bool PlaylistManager::toggleMute() {
  auto &mixer = AlsaMixer::getInstance();
  return mixer.toggleMute();
}

bool PlaylistManager::isMuted() const {
  auto &mixer = AlsaMixer::getInstance();
  return mixer.isMuted();
}

bool PlaylistManager::isPlaying() const {
  if (activeSocket_.empty()) {
    return false;
  }
  auto status = getStatus();
  return status.get("playing", false).asBool();
}

Json::Value PlaylistManager::getStatus() const {
  if (activeSocket_.empty()) {
    Json::Value status;
    status["playing"] = false;
    status["reason"] = "no_active_video";
    return status;
  }
  return playbackStatus_.getStatus(activeSocket_);
}

double PlaylistManager::getCurrentTime() const {
  auto status = getStatus();
  return status.get("currentTime", 0.0).asDouble();
}

double PlaylistManager::getDuration() const {
  auto status = getStatus();
  return status.get("duration", 0.0).asDouble();
}

void PlaylistManager::setLoopMode(bool enabled) {
  isLooping_ = enabled;
  std::cout << "[PlaylistManager] Loop mode: " << (enabled ? "ON" : "OFF")
            << std::endl;
}

void PlaylistManager::setShuffleMode(bool enabled) {
  isShuffled_ = enabled;
  std::cout << "[PlaylistManager] Shuffle mode: " << (enabled ? "ON" : "OFF")
            << std::endl;
}

void PlaylistManager::onTrackFinished() {
  std::cout << "[PlaylistManager] Track finished: " << currentIndex_ + 1
            << std::endl;
  if (isLooping_ ||
      currentIndex_ + 1 < static_cast<int>(currentPlaylist_.size())) {
    if (isLooping_ &&
        currentIndex_ + 1 >= static_cast<int>(currentPlaylist_.size())) {
      loadTrack(0);
    } else {
      loadTrack(currentIndex_ + 1);
    }
  } else {
    std::cout << "[PlaylistManager] Playlist finished" << std::endl;
    currentIndex_ = -1;
    activeSocket_.clear();
  }
}

std::vector<std::string> PlaylistManager::getFilePaths() const {
  std::vector<std::string> paths;
  paths.reserve(currentPlaylist_.size());
  for (const auto &track : currentPlaylist_) {
    paths.push_back(track.filePath);
  }
  return paths;
}
