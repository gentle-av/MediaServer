#include "services/PlayerService.h"
#include "player/Player.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

void PlayerService::onTrackLoaded() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_ && manualSwitch_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    internalPlayer_->play();
  }
  manualSwitch_ = false;
  isSwitching_ = false;
}

void PlayerService::onTrackEnd() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (isSwitching_ || manualSwitch_.load()) {
    return;
  }
  if (currentIndex_ < 0) {
    return;
  }
  if (currentIndex_ + 1 >= (int)playlist_.size()) {
    currentIndex_ = -1;
    return;
  }
  isSwitching_ = true;
  currentIndex_++;
  if (internalPlayer_) {
    internalPlayer_->playFile(playlist_[currentIndex_]);
  }
  isSwitching_ = false;
}

void PlayerService::playTrack(int index) {
  if (index < 0 || index >= (int)playlist_.size()) {
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    return;
  }
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  currentIndex_ = index;
  if (internalPlayer_) {
    internalPlayer_->playFile(playlist_[currentIndex_]);
  }
}

void PlayerService::next() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (isSwitching_) {
    return;
  }
  if (currentIndex_ + 1 >= (int)playlist_.size()) {
    return;
  }
  isSwitching_ = true;
  manualSwitch_ = true;
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  currentIndex_++;
  if (internalPlayer_) {
    internalPlayer_->playFile(playlist_[currentIndex_]);
  }
}

void PlayerService::previous() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (isSwitching_) {
    return;
  }
  if (currentIndex_ - 1 >= 0) {
    manualSwitch_ = true;
    if (internalPlayer_) {
      internalPlayer_->stop();
    }
    playTrack(currentIndex_ - 1);
  }
}

void PlayerService::setPlaylist(const std::vector<std::string> &tracks) {
  std::lock_guard<std::mutex> lock(mutex_);
  playlist_ = tracks;
  currentIndex_ = -1;
  manualSwitch_ = false;
  isSwitching_ = false;
  if (!playlist_.empty()) {
    manualSwitch_ = true;
    playTrack(0);
  }
}

void PlayerService::playIndex(int index) {
  if (index < 0 || index >= (int)playlist_.size()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    isSwitching_ = true;
    manualSwitch_ = true;
    if (internalPlayer_) {
      internalPlayer_->stop();
    }
    currentIndex_ = index;
  }
  if (internalPlayer_) {
    internalPlayer_->playFile(playlist_[index]);
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    isSwitching_ = false;
  }
}

PlayerService::PlayerService(int port)
    : currentIndex_(-1), isSwitching_(false), manualSwitch_(false) {
  internalPlayer_ = std::make_shared<Player>();
  internalPlayer_->setOnTrackEnd([this]() { onTrackEnd(); });
  internalPlayer_->setOnTrackLoaded([this]() { onTrackLoaded(); });
}

PlayerService::~PlayerService() {}

void PlayerService::play() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_) {
    internalPlayer_->play();
  }
}

void PlayerService::pause() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_) {
    internalPlayer_->pause();
  }
}

void PlayerService::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  currentIndex_ = -1;
  manualSwitch_ = false;
  isSwitching_ = false;
  playlist_.clear();
}

void PlayerService::addToPlaylist(const std::string &track) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::filesystem::exists(track)) {
    playlist_.push_back(track);
  }
}

void PlayerService::addAfterCurrent(const std::string &track) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::filesystem::exists(track) && currentIndex_ >= 0) {
    playlist_.insert(playlist_.begin() + currentIndex_ + 1, track);
  }
}

void PlayerService::replacePlaylist(const std::vector<std::string> &tracks) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  playlist_ = tracks;
  currentIndex_ = -1;
  manualSwitch_ = false;
  isSwitching_ = false;
  if (!playlist_.empty()) {
    currentIndex_ = 0;
    manualSwitch_ = true;
    if (internalPlayer_) {
      internalPlayer_->playFile(playlist_[0]);
    }
  }
}

void PlayerService::replacePlaylistWithTrack(const std::string &track) {
  setPlaylist({track});
}

void PlayerService::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  playlist_.clear();
  currentIndex_ = -1;
  manualSwitch_ = false;
  isSwitching_ = false;
}

void PlayerService::removeFromPlaylist(int index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index < 0 || index >= (int)playlist_.size()) {
    return;
  }
  playlist_.erase(playlist_.begin() + index);
  if (currentIndex_ > index) {
    currentIndex_--;
  } else if (currentIndex_ == index) {
    if (playlist_.empty()) {
      currentIndex_ = -1;
      manualSwitch_ = false;
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
  bool isPlaying = false;
  if (internalPlayer_) {
    isPlaying = internalPlayer_->isPlaying();
  }
  data["isPlaying"] = isPlaying;
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
  if (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size()) {
    std::string trackPath = playlist_[currentIndex_];
    data["track"] = getTrackTitle(trackPath);
    data["path"] = trackPath;
  } else {
    data["track"] = "";
    data["path"] = "";
  }
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

std::string PlayerService::getTrackTitle(const std::string &trackPath) {
  if (musicDb_) {
    MusicMetadata metadata;
    if (musicDb_->getMetadata(trackPath, metadata)) {
      if (!metadata.title.empty() && metadata.title != "Unknown") {
        return metadata.title;
      }
    }
  }
  std::filesystem::path path(trackPath);
  std::string filename = path.stem().string();
  std::regex trackPrefix(R"(^\d{1,2}[\.\-\s]+\s*)");
  std::string cleanTitle = std::regex_replace(filename, trackPrefix, "");
  return cleanTitle.empty() ? filename : cleanTitle;
}
