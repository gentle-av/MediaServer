#include "Musium.h"
#include <filesystem>
#include <iostream>

Musium::Musium() : mpv_(nullptr), running_(false), currentIndex_(-1) {
  initMpv();
}

Musium::~Musium() {
  stop();
  destroyMpv();
}

void Musium::initMpv() {
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
  eventThread_ = std::thread(&Musium::eventLoop, this);
}

void Musium::destroyMpv() {
  running_ = false;
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}

void Musium::eventLoop() {
  while (running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.1);
    if (!event)
      continue;
    if (event->event_id == MPV_EVENT_END_FILE) {
      loadNextTrack();
    } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    }
  }
}

void Musium::loadTrack(int index) {
  if (index < 0 || index >= (int)playlist_.size()) {
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    std::cerr << "[ERROR] File not found: " << playlist_[index] << std::endl;
    loadNextTrack();
    return;
  }
  currentIndex_ = index;
  const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
  mpv_command_async(mpv_, 0, args);
  std::cout << "[DEBUG] Loading: " << playlist_[currentIndex_] << std::endl;
}

void Musium::loadNextTrack() {
  int nextIndex = currentIndex_ + 1;
  if (nextIndex < (int)playlist_.size() && nextIndex >= 0) {
    loadTrack(nextIndex);
  } else {
    currentIndex_ = -1;
  }
}

bool Musium::start() { return mpv_ != nullptr; }

void Musium::stop() {
  if (mpv_) {
    const char *args[] = {"stop", NULL};
    mpv_command_async(mpv_, 0, args);
  }
  currentIndex_ = -1;
}

void Musium::play() {
  if (!mpv_)
    return;
  int pause = 0;
  mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
}

void Musium::pause() {
  if (!mpv_)
    return;
  int pause = 1;
  mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
}

void Musium::setPlaylist(const std::vector<std::string> &tracks) {
  playlist_ = tracks;
  currentIndex_ = -1;
  if (!playlist_.empty()) {
    loadTrack(0);
  }
}

std::vector<std::string> Musium::getPlaylist() { return playlist_; }
