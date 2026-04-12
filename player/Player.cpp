#include "Player.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>

Player::Player(bool enableVideo)
    : mpv_(nullptr), running_(false), manualStop_(false), loading_(false),
      currentIndex_(-1), fullscreen_(false), videoEnabled_(enableVideo) {
  std::cout << "[Player] Constructor start, enableVideo=" << enableVideo
            << std::endl;
  initMpv(enableVideo);
  std::cout << "[Player] Constructor end" << std::endl;
}

Player::Player() : Player(false) {}

Player::~Player() {
  std::cout << "[Player] Destructor start" << std::endl;
  running_ = false;
  if (eventThread_.joinable()) {
    std::cout << "[Player] Joining event thread" << std::endl;
    eventThread_.join();
    std::cout << "[Player] Event thread joined" << std::endl;
  }
  if (mpv_) {
    std::cout << "[Player] Destroying mpv" << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
  std::cout << "[Player] Destructor end" << std::endl;
}

void Player::eventLoop() {
  std::cout << "[Player::eventLoop] Thread started" << std::endl;
  while (running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.1);
    if (event->event_id == MPV_EVENT_NONE)
      continue;
    if (event->event_id == MPV_EVENT_END_FILE) {
      std::cout << "[Player::eventLoop] END_FILE event" << std::endl;
      if (!manualStop_) {
        if (videoEnabled_ && playlist_.size() == 1) {
          break;
        }
        loadNextTrack();
      }
      manualStop_ = false;
    } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
      std::cout << "[Player::eventLoop] SHUTDOWN event" << std::endl;
      break;
    }
  }
  std::cout << "[Player::eventLoop] Thread exiting" << std::endl;
}

void Player::initMpv(bool enableVideo) {
  std::cout << "[Player::initMpv] Start" << std::endl;
  mpv_ = mpv_create();
  if (!mpv_) {
    std::cout << "[Player::initMpv] mpv_create failed" << std::endl;
    return;
  }
  std::cout << "[Player::initMpv] mpv_create success" << std::endl;
  mpv_set_option_string(mpv_, "terminal", "yes");
  mpv_set_option_string(mpv_, "msg-level", "all=error");
  mpv_set_option_string(mpv_, "volume", "100");
  mpv_set_option_string(mpv_, "cache", "yes");
  mpv_set_option_string(mpv_, "cache-secs", "2");
  mpv_set_option_string(mpv_, "demuxer-max-bytes", "50M");
  mpv_set_option_string(mpv_, "demuxer-max-back-bytes", "50M");
  mpv_set_option_string(mpv_, "cache-pause", "no");
  mpv_set_option_string(mpv_, "cache-pause-initial", "no");
  mpv_set_option_string(mpv_, "audio-buffer", "1");
  mpv_set_option_string(mpv_, "audio-exclusive", "no");
  mpv_set_option_string(mpv_, "audio-stream-silence", "yes");
  if (enableVideo) {
    mpv_set_option_string(mpv_, "video", "yes");
    mpv_set_option_string(mpv_, "vo", "gpu-next");
    mpv_set_option_string(mpv_, "osc", "yes");
    mpv_set_option_string(mpv_, "load-scripts", "yes");
    mpv_set_option_string(mpv_, "keepaspect-window", "yes");
    mpv_set_option_string(mpv_, "border", "yes");
    mpv_set_option_string(mpv_, "geometry", "50%x50%");
    mpv_set_option_string(mpv_, "cursor-autohide", "1000");
    mpv_set_option_string(mpv_, "window-minimized", "no");
    int fullscreen = 1;
    mpv_set_option(mpv_, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
    int enableInput = 1;
    mpv_set_option(mpv_, "input-default-bindings", MPV_FORMAT_FLAG,
                   &enableInput);
    mpv_set_option(mpv_, "input-vo-keyboard", MPV_FORMAT_FLAG, &enableInput);
  } else {
    mpv_set_option_string(mpv_, "video", "no");
    mpv_set_option_string(mpv_, "vo", "null");
    mpv_set_option_string(mpv_, "osc", "no");
  }
  int keepOpen = 1;
  mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
  std::cout << "[Player::initMpv] Initializing mpv" << std::endl;
  if (mpv_initialize(mpv_) < 0) {
    std::cout << "[Player::initMpv] mpv_initialize failed" << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    return;
  }
  std::cout << "[Player::initMpv] mpv_initialize success" << std::endl;
  running_ = true;
  std::cout << "[Player::initMpv] Starting event thread" << std::endl;
  eventThread_ = std::thread(&Player::eventLoop, this);
  std::cout << "[Player::initMpv] Done" << std::endl;
}

void Player::stop() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  manualStop_ = true;
  const char *args[] = {"stop", NULL};
  mpv_command(mpv_, args);
  currentIndex_ = -1;
}

void Player::play() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  int pause = 0;
  mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
}

void Player::pause() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
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

void Player::next() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-next", NULL};
    mpv_command(mpv_, args);
  }
}

void Player::previous() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-prev", NULL};
    mpv_command(mpv_, args);
  }
}

void Player::setFullscreen(bool fullscreen) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  int flag = fullscreen ? 1 : 0;
  mpv_set_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &flag);
}

void Player::seekTo(double position) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  if (position < 0)
    position = 0;
  const char *args[] = {"seek", std::to_string(position).c_str(), "absolute",
                        NULL};
  mpv_command(mpv_, args);
}

bool Player::isFullscreen() const {
  int val = 0;
  if (mpv_) {
    mpv_get_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &val);
  }
  return val != 0;
}

void Player::setVideoEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  if (enabled) {
    mpv_set_option_string(mpv_, "video", "yes");
    mpv_set_option_string(mpv_, "vo", "gpu-next");
    videoEnabled_ = true;
  } else {
    mpv_set_option_string(mpv_, "video", "no");
    mpv_set_option_string(mpv_, "vo", "null");
    videoEnabled_ = false;
  }
}

void Player::stopAsync() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  manualStop_ = true;
  const char *args[] = {"stop", NULL};
  mpv_command(mpv_, args);
}

void Player::playAsync() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  if (!playlist_.empty() && currentIndex_ < 0) {
    currentIndex_ = 0;
    const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
    mpv_command(mpv_, args);
  } else {
    int pause = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  }
}

void Player::setPlaylistAsync(const std::vector<std::string> &tracks) {
  playlist_ = tracks;
  currentIndex_ = -1;
}

void Player::loadTrack(int index) {
  if (loading_)
    return;
  loading_ = true;
  if (index < 0 || index >= (int)playlist_.size()) {
    loading_ = false;
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    loading_ = false;
    loadNextTrack();
    return;
  }
  currentIndex_ = index;
  manualStop_ = false;
  {
    std::lock_guard<std::mutex> lock(mpvMutex_);
    if (mpv_) {
      const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
      mpv_command(mpv_, args);
    }
  }
  loading_ = false;
}

void Player::loadNextTrack() {
  int nextIndex = currentIndex_ + 1;
  if (nextIndex < (int)playlist_.size() && nextIndex >= 0) {
    loadTrack(nextIndex);
  } else {
    currentIndex_ = -1;
  }
}

void Player::forceQuit() {
  std::cout << "[Player::forceQuit] Called" << std::endl;
  running_ = false;
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}
