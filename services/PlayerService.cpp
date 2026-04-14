// PlayerService.cpp - добавьте реализацию onTrackLoaded в начало файла
#include "services/PlayerService.h"
#include "player/Player.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

PlayerService::PlayerService(int port)
    : currentIndex_(-1), isSwitching_(false), manualSwitch_(false) {
  std::cout << "[PlayerService] Constructor" << std::endl;
}

PlayerService::~PlayerService() {
  std::cout << "[PlayerService] Destructor" << std::endl;
}

void PlayerService::onTrackLoaded() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] onTrackLoaded, manualSwitch="
            << manualSwitch_.load() << std::endl;
  manualSwitch_ = false;
}

void PlayerService::onTrackEnd() {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] onTrackEnd, currentIndex=" << currentIndex_
            << ", isSwitching=" << isSwitching_.load()
            << ", manualSwitch=" << manualSwitch_.load() << std::endl;
  if (isSwitching_) {
    std::cout << "[PlayerService] onTrackEnd: already switching, ignoring"
              << std::endl;
    return;
  }
  if (manualSwitch_) {
    std::cout << "[PlayerService] onTrackEnd: manual switch was done, ignoring "
                 "auto next"
              << std::endl;
    return;
  }
  if (currentIndex_ + 1 < (int)playlist_.size()) {
    isSwitching_ = true;
    playTrack(currentIndex_ + 1);
    isSwitching_ = false;
  } else {
    std::cout << "[PlayerService] End of playlist" << std::endl;
    currentIndex_ = -1;
  }
}

void PlayerService::playTrack(int index) {
  std::cout << "[PlayerService] playTrack: index=" << index
            << ", currentIndex=" << currentIndex_ << std::endl;
  if (index < 0 || index >= (int)playlist_.size()) {
    std::cout << "[PlayerService] playTrack: invalid index" << std::endl;
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    std::cout << "[PlayerService] playTrack: file not found: "
              << playlist_[index] << std::endl;
    return;
  }
  if (currentIndex_ == index) {
    std::cout << "[PlayerService] playTrack: already on this track, ignoring"
              << std::endl;
    return;
  }
  currentIndex_ = index;
  std::cout << "[PlayerService] playTrack: playing " << playlist_[currentIndex_]
            << std::endl;
  if (internalPlayer_) {
    internalPlayer_->playFile(playlist_[currentIndex_]);
  }
}

void PlayerService::play() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] play" << std::endl;
  if (internalPlayer_) {
    internalPlayer_->play();
  }
}

void PlayerService::pause() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] pause" << std::endl;
  if (internalPlayer_) {
    internalPlayer_->pause();
  }
}

void PlayerService::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] stop" << std::endl;
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  currentIndex_ = -1;
}

void PlayerService::next() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] next, currentIndex=" << currentIndex_
            << ", isSwitching=" << isSwitching_.load() << std::endl;
  if (isSwitching_) {
    std::cout << "[PlayerService] next: already switching, ignoring"
              << std::endl;
    return;
  }
  if (currentIndex_ + 1 < (int)playlist_.size()) {
    isSwitching_ = true;
    manualSwitch_ = true;
    if (internalPlayer_) {
      internalPlayer_->stop();
    }
    playTrack(currentIndex_ + 1);
    isSwitching_ = false;
  } else {
    std::cout << "[PlayerService] next: at end of playlist" << std::endl;
  }
}

void PlayerService::previous() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] previous, currentIndex=" << currentIndex_
            << ", isSwitching=" << isSwitching_.load() << std::endl;
  if (isSwitching_) {
    std::cout << "[PlayerService] previous: already switching, ignoring"
              << std::endl;
    return;
  }
  if (currentIndex_ - 1 >= 0) {
    isSwitching_ = true;
    manualSwitch_ = true;
    if (internalPlayer_) {
      internalPlayer_->stop();
    }
    playTrack(currentIndex_ - 1);
    isSwitching_ = false;
  } else {
    std::cout << "[PlayerService] previous: at start of playlist" << std::endl;
  }
}

void PlayerService::setPlaylist(const std::vector<std::string> &tracks) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] setPlaylist, size=" << tracks.size()
            << std::endl;
  playlist_ = tracks;
  currentIndex_ = -1;
  if (!playlist_.empty()) {
    manualSwitch_ = true;
    playTrack(0);
  }
}

void PlayerService::addToPlaylist(const std::string &track) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::filesystem::exists(track)) {
    playlist_.push_back(track);
    std::cout << "[PlayerService] addToPlaylist: " << track << std::endl;
  }
}

void PlayerService::addAfterCurrent(const std::string &track) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::filesystem::exists(track) && currentIndex_ >= 0) {
    playlist_.insert(playlist_.begin() + currentIndex_ + 1, track);
    std::cout << "[PlayerService] addAfterCurrent: " << track << std::endl;
  }
}

void PlayerService::replacePlaylist(const std::vector<std::string> &tracks) {
  setPlaylist(tracks);
}

void PlayerService::replacePlaylistWithTrack(const std::string &track) {
  setPlaylist({track});
}

void PlayerService::playIndex(int index) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] playIndex: " << index << std::endl;
  if (index >= 0 && index < (int)playlist_.size()) {
    manualSwitch_ = true;
    playTrack(index);
  }
}

void PlayerService::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] clear" << std::endl;
  playlist_.clear();
  currentIndex_ = -1;
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
}

void PlayerService::removeFromPlaylist(int index) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "[PlayerService] removeFromPlaylist: " << index << std::endl;
  if (index < 0 || index >= (int)playlist_.size()) {
    return;
  }
  playlist_.erase(playlist_.begin() + index);
  if (currentIndex_ > index) {
    currentIndex_--;
  } else if (currentIndex_ == index) {
    if (playlist_.empty()) {
      currentIndex_ = -1;
      if (internalPlayer_) {
        internalPlayer_->stop();
      }
    } else if (currentIndex_ >= (int)playlist_.size()) {
      currentIndex_ = (int)playlist_.size() - 1;
      manualSwitch_ = true;
      playTrack(currentIndex_);
    } else {
      manualSwitch_ = true;
      playTrack(currentIndex_);
    }
  }
}

Json::Value PlayerService::getPlaylist() {
  std::lock_guard<std::mutex> lock(mutex_);
  Json::Value result(Json::arrayValue);
  for (const auto &track : playlist_) {
    result.append(track);
  }
  return result;
}

Json::Value PlayerService::getPlaybackState() {
  std::lock_guard<std::mutex> lock(mutex_);
  Json::Value data;
  data["isPlaying"] = internalPlayer_ ? internalPlayer_->isPlaying() : false;
  data["currentTrack"] =
      currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size()
          ? playlist_[currentIndex_]
          : "";
  data["currentIndex"] = currentIndex_;
  data["totalTracks"] = (int)playlist_.size();
  return data;
}

Json::Value PlayerService::getCurrentTrack() {
  std::lock_guard<std::mutex> lock(mutex_);
  Json::Value data;
  data["track"] = currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size()
                      ? playlist_[currentIndex_]
                      : "";
  return data;
}

Json::Value PlayerService::getCurrentTime() {
  Json::Value data;
  if (internalPlayer_) {
    data["currentTime"] = internalPlayer_->getCurrentTime();
    data["duration"] = internalPlayer_->getDuration();
  } else {
    data["currentTime"] = 0;
    data["duration"] = 0;
  }
  return data;
}

void PlayerService::setInternalPlayer(std::shared_ptr<Player> player) {
  internalPlayer_ = player;
  internalPlayer_->setOnTrackEnd([this]() { onTrackEnd(); });
  internalPlayer_->setOnTrackLoaded([this]() { onTrackLoaded(); });
}
