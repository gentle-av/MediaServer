#include "Player.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>

Player::Player()
    : mpv_(nullptr), running_(false), stopping_(false), manualStop_(false),
      loading_(false), currentIndex_(-1), fullscreen_(false),
      videoMode_(false) {
  std::cout << "[Player] Constructor START" << std::endl;
  initMpv();
  std::cout << "[Player] Constructor END" << std::endl;
}

Player::~Player() {
  std::cout << "[Player] Destructor START" << std::endl;
  stopping_ = true;
  running_ = false;
  if (eventThread_.joinable()) {
    std::cout << "[Player] Waiting for event thread..." << std::endl;
    eventThread_.join();
    std::cout << "[Player] Event thread joined" << std::endl;
  }
  if (mpv_) {
    std::cout << "[Player] Terminating mpv..." << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    std::cout << "[Player] Mpv terminated" << std::endl;
  }
  std::cout << "[Player] Destructor END" << std::endl;
}

void Player::eventLoop() {
  std::cout << "[Player::eventLoop] START" << std::endl;
  while (!stopping_ && running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.05);
    double time = 0;
    mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &time);
    if (time != currentTime_.load()) {
      currentTime_ = time;
      std::cout << "[Player::eventLoop] time=" << time << std::endl;
    }
    double duration = 0;
    mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &duration);
    if (duration > 0 && duration != duration_.load()) {
      duration_ = duration;
      std::cout << "[Player::eventLoop] duration=" << duration << std::endl;
    }
    int pause = 1;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
    isPlaying_ = (pause == 0);
    if (!event || event->event_id == MPV_EVENT_NONE) {
      continue;
    }
    if (event->event_id == MPV_EVENT_END_FILE) {
      if (!manualStop_) {
        if (videoMode_ && playlist_.size() == 1) {
          break;
        }
        loadNextTrack();
      }
      manualStop_ = false;
    } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    }
  }
  std::cout << "[Player::eventLoop] END" << std::endl;
}

void Player::forceQuit() {
  std::cout << "[Player::forceQuit] START" << std::endl;
  stopping_ = true;
  running_ = false;
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
  std::cout << "[Player::forceQuit] END" << std::endl;
}

void Player::play() {
  std::cout << "[Player::play] START, mpv_=" << mpv_
            << ", running_=" << running_ << std::endl;
  if (!mpv_) {
    std::cout << "[Player::play] mpv_ is NULL" << std::endl;
    return;
  }
  if (!running_) {
    std::cout << "[Player::play] not running" << std::endl;
    return;
  }
  std::lock_guard<std::mutex> lock(mpvMutex_);
  const char *args[] = {"set", "pause", "no", NULL};
  int err = mpv_command(mpv_, args);
  std::cout << "[Player::play] mpv_command result=" << err << std::endl;
  std::cout << "[Player::play] END" << std::endl;
}

void Player::loadTrack(int index) {
  if (!mpvValid_)
    return;
  std::cout << "[Player::loadTrack] START, index=" << index
            << ", loading_=" << loading_.load() << ", mpv_=" << mpv_
            << std::endl;
  if (loading_)
    return;
  loading_ = true;
  if (index < 0 || index >= (int)playlist_.size()) {
    std::cout << "[Player::loadTrack] index out of range" << std::endl;
    loading_ = false;
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    std::cout << "[Player::loadTrack] file not exist: " << playlist_[index]
              << std::endl;
    loading_ = false;
    loadNextTrack();
    return;
  }
  currentIndex_ = index;
  manualStop_ = false;
  {
    std::lock_guard<std::mutex> lock(mpvMutex_);
    if (mpv_ && running_) {
      std::cout << "[Player::loadTrack] loading file: "
                << playlist_[currentIndex_] << std::endl;
      const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
      int err = mpv_command(mpv_, args);
      std::cout << "[Player::loadTrack] mpv_command returned: " << err
                << std::endl;
    } else {
      std::cout << "[Player::loadTrack] mpv_=" << mpv_
                << ", running_=" << running_ << std::endl;
    }
  }
  loading_ = false;
  std::cout << "[Player::loadTrack] END" << std::endl;
}

void Player::pause() {
  std::cout << "[Player::pause] START" << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  const char *args[] = {"set", "pause", "yes", NULL};
  mpv_command(mpv_, args);
  std::cout << "[Player::pause] END" << std::endl;
}

bool Player::isFullscreen() const { return fullscreen_.load(); }

void Player::initMpv() {
  std::cout << "[Player::initMpv] START" << std::endl;
  mpv_ = mpv_create();
  if (!mpv_) {
    std::cout << "[Player::initMpv] mpv_create FAILED" << std::endl;
    mpvValid_ = false;
    return;
  }
  mpvValid_ = true;
  std::cout << "[Player::initMpv] mpv_create SUCCESS, mpv_=" << mpv_
            << std::endl;
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
  mpv_set_option_string(mpv_, "video", "no");
  mpv_set_option_string(mpv_, "vo", "null");
  mpv_set_option_string(mpv_, "osc", "no");
  int keepOpen = 1;
  mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
  std::cout << "[Player::initMpv] Calling mpv_initialize..." << std::endl;
  if (mpv_initialize(mpv_) < 0) {
    std::cout << "[Player::initMpv] mpv_initialize FAILED" << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    return;
  }
  std::cout << "[Player::initMpv] mpv_initialize SUCCESS" << std::endl;
  mpv_observe_property(mpv_, 0, "time-pos", MPV_FORMAT_DOUBLE);
  mpv_observe_property(mpv_, 1, "duration", MPV_FORMAT_DOUBLE);
  mpv_observe_property(mpv_, 2, "pause", MPV_FORMAT_FLAG);
  running_ = true;
  eventThread_ = std::thread(&Player::eventLoop, this);
  std::cout << "[Player::initMpv] END" << std::endl;
}

void Player::stop() {
  std::cout << "[Player::stop] START, mpv_=" << mpv_ << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_) {
    std::cout << "[Player::stop] mpv_ is NULL" << std::endl;
    return;
  }
  manualStop_ = true;
  const char *args[] = {"stop", NULL};
  mpv_command(mpv_, args);
  currentIndex_ = -1;
  std::cout << "[Player::stop] END" << std::endl;
}

void Player::setPlaylist(const std::vector<std::string> &tracks) {
  std::cout << "[Player::setPlaylist] START, tracks.size=" << tracks.size()
            << ", mpv_=" << mpv_ << std::endl;
  playlist_ = tracks;
  currentIndex_ = -1;
  if (!playlist_.empty()) {
    std::cout << "[Player::setPlaylist] calling loadTrack(0)" << std::endl;
    loadTrack(0);
  }
  std::cout << "[Player::setPlaylist] END" << std::endl;
}

std::vector<std::string> Player::getPlaylist() { return playlist_; }

void Player::next() {
  std::cout << "[Player::next] START" << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-next", NULL};
    mpv_command(mpv_, args);
  }
  std::cout << "[Player::next] END" << std::endl;
}

void Player::previous() {
  std::cout << "[Player::previous] START" << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-prev", NULL};
    mpv_command(mpv_, args);
  }
  std::cout << "[Player::previous] END" << std::endl;
}

void Player::setFullscreen(bool fullscreen) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  int flag = fullscreen ? 1 : 0;
  mpv_set_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &flag);
}

void Player::seekTo(double position) {
  std::cout << "[Player::seekTo] START, position=" << position << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  if (position < 0)
    position = 0;
  const char *args[] = {"seek", std::to_string(position).c_str(), "absolute",
                        NULL};
  mpv_command(mpv_, args);
  std::cout << "[Player::seekTo] END" << std::endl;
}

void Player::setVideoMode(bool enabled) {
  std::cout << "[Player::setVideoMode] START, enabled=" << enabled
            << ", videoMode_=" << videoMode_ << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_) {
    std::cout << "[Player::setVideoMode] mpv_ is NULL" << std::endl;
    return;
  }
  if (enabled && !videoMode_) {
    const char *stop_args[] = {"stop", NULL};
    mpv_command(mpv_, stop_args);
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
    videoMode_ = true;
    if (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size()) {
      const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
      mpv_command(mpv_, args);
    }
  } else if (!enabled && videoMode_) {
    const char *stop_args[] = {"stop", NULL};
    mpv_command(mpv_, stop_args);
    mpv_set_option_string(mpv_, "video", "no");
    mpv_set_option_string(mpv_, "vo", "null");
    mpv_set_option_string(mpv_, "osc", "no");
    videoMode_ = false;
    if (currentIndex_ >= 0 && currentIndex_ < (int)playlist_.size()) {
      const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
      mpv_command(mpv_, args);
    }
  }
  std::cout << "[Player::setVideoMode] END" << std::endl;
}

void Player::loadNextTrack() {
  int nextIndex = currentIndex_ + 1;
  std::cout << "[Player::loadNextTrack] nextIndex=" << nextIndex << std::endl;
  if (nextIndex < (int)playlist_.size() && nextIndex >= 0) {
    loadTrack(nextIndex);
  } else {
    currentIndex_ = -1;
  }
}
