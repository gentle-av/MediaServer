#include "Player.h"
#include <filesystem>
#include <iostream>

Player::Player()
    : mpv_(nullptr), running_(false), manualStop_(false), currentIndex_(-1) {
  initMpv();
}

Player::~Player() {
  stop();
  destroyMpv();
}

void Player::initMpv() {
  mpv_ = mpv_create();
  if (!mpv_) {
    std::cerr << "[ERROR] Failed to create mpv handle" << std::endl;
    return;
  }
  mpv_set_option_string(mpv_, "video", "no");
  mpv_set_option_string(mpv_, "terminal", "yes");
  mpv_set_option_string(mpv_, "msg-level", "all=error");
  mpv_set_option_string(mpv_, "volume", "100");
  if (mpv_initialize(mpv_) < 0) {
    std::cerr << "[ERROR] Failed to initialize mpv" << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    return;
  }
  running_ = true;
  eventThread_ = std::thread(&Player::eventLoop, this);
}

void Player::destroyMpv() {
  running_ = false;
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}

void Player::eventLoop() {
  while (running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.1);
    if (!event)
      continue;
    if (event->event_id == MPV_EVENT_END_FILE) {
      std::cout << "[DEBUG] MPV_EVENT_END_FILE received, manualStop_="
                << manualStop_ << std::endl;
      if (!manualStop_) {
        std::cout << "[DEBUG] Calling loadNextTrack()" << std::endl;
        loadNextTrack();
      }
      manualStop_ = false;
    } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    }
  }
}

void Player::loadTrack(int index) {
  std::cout << "[DEBUG] loadTrack called with index=" << index
            << ", currentIndex_ before=" << currentIndex_
            << ", playlist_.size()=" << playlist_.size() << std::endl;
  if (index < 0 || index >= (int)playlist_.size()) {
    std::cout << "[DEBUG] loadTrack: index out of range, returning"
              << std::endl;
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    std::cerr << "[ERROR] File not found: " << playlist_[index] << std::endl;
    loadNextTrack();
    return;
  }
  currentIndex_ = index;
  manualStop_ = false;
  const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
  mpv_command_async(mpv_, 0, args);
  std::cout << "[DEBUG] Loading: " << playlist_[currentIndex_] << std::endl;
}

void Player::loadNextTrack() {
  int nextIndex = currentIndex_ + 1;
  std::cout << "[DEBUG] loadNextTrack: currentIndex_=" << currentIndex_
            << ", nextIndex=" << nextIndex
            << ", playlist_.size()=" << playlist_.size() << std::endl;
  if (nextIndex < (int)playlist_.size() && nextIndex >= 0) {
    loadTrack(nextIndex);
  } else {
    std::cout
        << "[DEBUG] loadNextTrack: no next track, setting currentIndex_=-1"
        << std::endl;
    currentIndex_ = -1;
  }
}

bool Player::start() { return mpv_ != nullptr; }

void Player::stop() {
  if (mpv_) {
    manualStop_ = true;
    const char *args[] = {"stop", NULL};
    mpv_command_async(mpv_, 0, args);
  }
  currentIndex_ = -1;
}

void Player::play() {
  if (!mpv_)
    return;
  int pause = 0;
  mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
}

void Player::pause() {
  if (!mpv_)
    return;
  int pause = 1;
  mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
}

void Player::setPlaylist(const std::vector<std::string> &tracks) {
  playlist_ = tracks;
  currentIndex_ = -1;
  if (!playlist_.empty()) {
    loadTrack(0);
  }
}

std::vector<std::string> Player::getPlaylist() { return playlist_; }
