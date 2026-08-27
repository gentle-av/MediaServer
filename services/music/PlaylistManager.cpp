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
  currentPlaylist = tracks;
  currentIndex = 0;
  if (shuffled && currentPlaylist.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist.begin(), currentPlaylist.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playPlaylist(Playlist &&playlist) {
  auto tracks = playlist.getAllTracks();
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty playlist" << std::endl;
    return false;
  }
  currentPlaylist = std::move(tracks);
  currentIndex = 0;
  if (shuffled && currentPlaylist.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist.begin(), currentPlaylist.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playTracks(const std::vector<MusicMetadata> &tracks) {
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty track list" << std::endl;
    return false;
  }
  currentPlaylist = tracks;
  currentIndex = 0;
  if (shuffled && currentPlaylist.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist.begin(), currentPlaylist.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playTracks(std::vector<MusicMetadata> &&tracks) {
  if (tracks.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty track list" << std::endl;
    return false;
  }
  currentPlaylist = std::move(tracks);
  currentIndex = 0;
  if (shuffled && currentPlaylist.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist.begin(), currentPlaylist.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::playFilePaths(const std::vector<std::string> &paths) {
  if (paths.empty()) {
    std::cerr << "[PlaylistManager] Cannot play empty file list" << std::endl;
    return false;
  }
  currentPlaylist.clear();
  currentPlaylist.reserve(paths.size());
  for (const auto &path : paths) {
    MusicMetadata meta;
    meta.filePath = path;
    currentPlaylist.push_back(meta);
  }
  currentIndex = 0;
  if (shuffled && currentPlaylist.size() > 1) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(currentPlaylist.begin(), currentPlaylist.end(), g);
  }
  return loadTrack(0);
}

bool PlaylistManager::loadTrack(int index) {
  if (index < 0 || index >= static_cast<int>(currentPlaylist.size())) {
    std::cerr << "[PlaylistManager] Invalid track index: " << index
              << std::endl;
    return false;
  }
  const auto &track = currentPlaylist[index];
  currentIndex = index;
  Json::Value response =
      videoControl.handleOpen(track.filePath, activeSocket, true);
  bool success = response.get("success", false).asBool();
  if (success) {
    std::cout << "[PlaylistManager] Playing track " << index + 1 << "/"
              << currentPlaylist.size() << ": "
              << (track.title.empty() ? track.filePath : track.title)
              << std::endl;
  } else {
    std::cerr << "[PlaylistManager] Failed to load track: " << track.filePath
              << " - " << response.get("error", "unknown error").asString()
              << std::endl;
    if (index + 1 < static_cast<int>(currentPlaylist.size())) {
      return loadTrack(index + 1);
    }
  }
  return success;
}

void PlaylistManager::pause() {
  videoControl.handleControl("pause", activeSocket);
}

void PlaylistManager::resume() {
  videoControl.handleControl("play", activeSocket);
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
  videoControl.handleClose(activeSocket);
  currentIndex = -1;
}

bool PlaylistManager::seek(double seconds) {
  if (activeSocket.empty()) {
    std::cerr << "[PlaylistManager] No active video to seek" << std::endl;
    return false;
  }
  Json::Value response = videoControl.handleSeek(seconds, activeSocket);
  return response.get("success", false).asBool();
}

bool PlaylistManager::next() {
  if (currentPlaylist.empty()) {
    return false;
  }
  int nextIndex = currentIndex + 1;
  if (nextIndex >= static_cast<int>(currentPlaylist.size())) {
    if (looping) {
      nextIndex = 0;
    } else {
      std::cout << "[PlaylistManager] End of playlist" << std::endl;
      return false;
    }
  }
  return loadTrack(nextIndex);
}

bool PlaylistManager::previous() {
  if (currentPlaylist.empty() || currentIndex <= 0) {
    return false;
  }
  return loadTrack(currentIndex - 1);
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
  if (activeSocket.empty()) {
    return false;
  }
  auto status = getStatus();
  return status.get("playing", false).asBool();
}

Json::Value PlaylistManager::getStatus() const {
  if (activeSocket.empty()) {
    Json::Value status;
    status["playing"] = false;
    status["reason"] = "no_active_video";
    return status;
  }
  return playbackStatus.getStatus(activeSocket);
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
  looping = enabled;
  std::cout << "[PlaylistManager] Loop mode: " << (enabled ? "ON" : "OFF")
            << std::endl;
}

void PlaylistManager::setShuffleMode(bool enabled) {
  shuffled = enabled;
  std::cout << "[PlaylistManager] Shuffle mode: " << (enabled ? "ON" : "OFF")
            << std::endl;
}

void PlaylistManager::onTrackFinished() {
  std::cout << "[PlaylistManager] Track finished: " << currentIndex + 1
            << std::endl;
  if (looping || currentIndex + 1 < static_cast<int>(currentPlaylist.size())) {
    if (looping &&
        currentIndex + 1 >= static_cast<int>(currentPlaylist.size())) {
      loadTrack(0);
    } else {
      loadTrack(currentIndex + 1);
    }
  } else {
    std::cout << "[PlaylistManager] Playlist finished" << std::endl;
    currentIndex = -1;
    activeSocket.clear();
  }
}

std::vector<std::string> PlaylistManager::getFilePaths() const {
  std::vector<std::string> paths;
  paths.reserve(currentPlaylist.size());
  for (const auto &track : currentPlaylist) {
    paths.push_back(track.filePath);
  }
  return paths;
}
