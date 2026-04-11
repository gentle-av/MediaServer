#include "Player.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>

Player::Player() : Player(false) {}

Player::Player(bool enableVideo)
    : mpv_(nullptr), running_(false), manualStop_(false), loading_(false),
      currentIndex_(-1), fullscreen_(false), videoEnabled_(enableVideo) {
  initMpv(enableVideo);
}

Player::~Player() { forceQuit(); }

void Player::initMpv(bool enableVideo) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  mpv_ = mpv_create();
  if (!mpv_) {
    std::cerr << "[ERROR] Failed to create mpv handle" << std::endl;
    return;
  }
  std::cout << "[DEBUG] initMpv: enableVideo=" << enableVideo << std::endl;
  mpv_set_option_string(mpv_, "terminal", "yes");
  mpv_set_option_string(mpv_, "msg-level", "all=error");
  mpv_set_option_string(mpv_, "volume", "100");
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
    mpv_set_option_string(mpv_, "input-conf", "");
    mpv_set_option_string(mpv_, "input-cursor", "yes");
    std::cout << "[DEBUG] Video enabled with vo=gpu-next, fullscreen=1"
              << std::endl;
  } else {
    mpv_set_option_string(mpv_, "video", "no");
    mpv_set_option_string(mpv_, "vo", "null");
    mpv_set_option_string(mpv_, "osc", "no");
    std::cout << "[DEBUG] Video disabled" << std::endl;
  }
  int keepOpen = 1;
  mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
  if (mpv_initialize(mpv_) < 0) {
    std::cerr << "[ERROR] Failed to initialize mpv" << std::endl;
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    return;
  }
  std::cout << "[DEBUG] initMpv: mpv initialized" << std::endl;
  mpv_observe_property(mpv_, 0, "fullscreen", MPV_FORMAT_FLAG);
  running_ = true;
  eventThread_ = std::thread(&Player::eventLoop, this);
}

void Player::destroyMpv() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_) {
    int keepOpen = 0;
    mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
    const char *args[] = {"stop", NULL};
    mpv_command(mpv_, args);
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}

void Player::forceQuit() {
  std::cout << "[DEBUG] forceQuit called" << std::endl;
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(mpvMutex_);
    if (mpv_) {
      const char *args[] = {"quit", NULL};
      mpv_command(mpv_, args);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      mpv_terminate_destroy(mpv_);
      mpv_ = nullptr;
    }
  }
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
}

void Player::eventLoop() {
  auto lastActivity = std::chrono::steady_clock::now();
  while (running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.5);
    if (!event) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity)
              .count();
      if (elapsed > 300 && videoEnabled_) {
        std::cout << "[DEBUG] No activity for 5 minutes, shutting down"
                  << std::endl;
        break;
      }
      continue;
    }
    lastActivity = std::chrono::steady_clock::now();
    if (event->event_id == MPV_EVENT_END_FILE) {
      std::cout << "[DEBUG] MPV_EVENT_END_FILE received" << std::endl;
      if (!manualStop_) {
        if (videoEnabled_ && playlist_.size() == 1) {
          std::cout << "[DEBUG] Video finished, auto-quitting" << std::endl;
          forceQuit();
          break;
        }
        loadNextTrack();
      }
      manualStop_ = false;
    } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      mpv_event_property *prop = (mpv_event_property *)event->data;
      if (prop->name && strcmp(prop->name, "fullscreen") == 0 &&
          prop->format == MPV_FORMAT_FLAG) {
        int val;
        mpv_get_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &val);
        fullscreen_ = (val != 0);
        std::cout << "[DEBUG] Fullscreen changed to: " << fullscreen_
                  << std::endl;
      }
    }
  }
  std::cout << "[DEBUG] Exiting event loop, stopping playback" << std::endl;
  stop();
}

void Player::loadTrack(int index) {
  if (loading_) {
    std::cout << "[DEBUG] loadTrack: already loading, skip" << std::endl;
    return;
  }
  loading_ = true;
  std::cout << "[DEBUG] loadTrack called with index=" << index
            << ", currentIndex_ before=" << currentIndex_
            << ", playlist_.size()=" << playlist_.size() << std::endl;
  if (index < 0 || index >= (int)playlist_.size()) {
    std::cout << "[DEBUG] loadTrack: index out of range, returning"
              << std::endl;
    loading_ = false;
    return;
  }
  if (!std::filesystem::exists(playlist_[index])) {
    std::cerr << "[ERROR] File not found: " << playlist_[index] << std::endl;
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
      mpv_command_async(mpv_, 0, args);
    }
  }
  std::cout << "[DEBUG] Loading: " << playlist_[currentIndex_] << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  loading_ = false;
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
  std::cout << "[DEBUG] Player::stop called, mpv_=" << mpv_ << std::endl;
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_) {
    manualStop_ = true;
    const char *args[] = {"stop", NULL};
    mpv_command_async(mpv_, 0, args);
    std::cout << "[DEBUG] Player::stop: mpv stop command sent" << std::endl;
  }
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
    mpv_command_async(mpv_, 0, args);
  }
}

void Player::previous() {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-prev", NULL};
    mpv_command_async(mpv_, 0, args);
  }
}

void Player::setFullscreen(bool fullscreen) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_) {
    std::cout << "[DEBUG] setFullscreen: mpv_ is null" << std::endl;
    return;
  }
  std::cout << "[DEBUG] setFullscreen: fullscreen=" << fullscreen << std::endl;
  int flag = fullscreen ? 1 : 0;
  int result = mpv_set_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &flag);
  std::cout << "[DEBUG] setFullscreen: mpv_set_property result=" << result
            << std::endl;
}

void Player::seekForward(int seconds) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  const char *args[] = {"seek", std::to_string(seconds).c_str(), "relative",
                        NULL};
  mpv_command_async(mpv_, 0, args);
}

void Player::seekBackward(int seconds) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  const char *args[] = {"seek", std::to_string(-seconds).c_str(), "relative",
                        NULL};
  mpv_command_async(mpv_, 0, args);
}

void Player::seekTo(double position) {
  std::lock_guard<std::mutex> lock(mpvMutex_);
  if (!mpv_)
    return;
  if (position < 0)
    position = 0;
  const char *args[] = {"seek", std::to_string(position).c_str(), "absolute",
                        NULL};
  mpv_command_async(mpv_, 0, args);
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
  std::cout << "[DEBUG] setVideoEnabled: " << enabled << std::endl;
  if (enabled) {
    mpv_set_option_string(mpv_, "video", "yes");
    mpv_set_option_string(mpv_, "vo", "gpu-next");
    mpv_set_option_string(mpv_, "osc", "yes");
    mpv_set_option_string(mpv_, "load-scripts", "yes");
    mpv_set_option_string(mpv_, "keepaspect-window", "yes");
    mpv_set_option_string(mpv_, "border", "yes");
    mpv_set_option_string(mpv_, "geometry", "50%x50%");
    mpv_set_option_string(mpv_, "cursor-autohide", "1000");
    mpv_set_option_string(mpv_, "window-minimized", "no");
    videoEnabled_ = true;
    std::cout << "[DEBUG] Video enabled" << std::endl;
  } else {
    mpv_set_option_string(mpv_, "video", "no");
    mpv_set_option_string(mpv_, "vo", "null");
    mpv_set_option_string(mpv_, "osc", "no");
    videoEnabled_ = false;
    std::cout << "[DEBUG] Video disabled" << std::endl;
  }
}

void Player::observeProperties() {
  mpv_observe_property(mpv_, 0, "fullscreen", MPV_FORMAT_FLAG);
}
