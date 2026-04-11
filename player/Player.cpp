#include "Player.h"
#include <cstring>
#include <filesystem>
#include <iostream>

Player::Player(bool enableVideo)
    : mpv_(nullptr), running_(false), manualStop_(false), loading_(false),
      currentIndex_(-1), fullscreen_(false), videoEnabled_(enableVideo) {
  initMpv(enableVideo);
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
  const char *args[] = {"loadfile", playlist_[currentIndex_].c_str(), NULL};
  mpv_command_async(mpv_, 0, args);
  std::cout << "[DEBUG] Loading: " << playlist_[currentIndex_] << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  loading_ = false;
}

void Player::eventLoop() {
  while (running_) {
    mpv_event *event = mpv_wait_event(mpv_, 0.5);
    if (!event)
      continue;
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
    }
  }
}

void Player::stop() {
  if (mpv_) {
    manualStop_ = true;
    const char *args[] = {"stop", NULL};
    mpv_command_async(mpv_, 0, args);
  }
  currentIndex_ = -1;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

void Player::next() {
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-next", NULL};
    mpv_command_async(mpv_, 0, args);
  }
}

void Player::previous() {
  if (mpv_ && !loading_) {
    const char *args[] = {"playlist-prev", NULL};
    mpv_command_async(mpv_, 0, args);
  }
}

Player::Player() : Player(false) {}

Player::~Player() {
  stop();
  destroyMpv();
}

void Player::initMpv(bool enableVideo) {
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
  running_ = false;
  if (mpv_) {
    int keepOpen = 0;
    mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
    const char *args[] = {"stop", NULL};
    mpv_command(mpv_, args);
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
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

void Player::setPlaylist(const std::vector<std::string> &tracks) {
  playlist_ = tracks;
  currentIndex_ = -1;
  if (!playlist_.empty()) {
    loadTrack(0);
  }
}

std::vector<std::string> Player::getPlaylist() { return playlist_; }

void Player::setFullscreen(bool fullscreen) {
  if (!mpv_) {
    std::cout << "[DEBUG] setFullscreen: mpv_ is null, ignoring" << std::endl;
    return;
  }
  std::cout << "[DEBUG] setFullscreen: fullscreen=" << fullscreen << std::endl;
  int flag = fullscreen ? 1 : 0;
  int result = mpv_set_property(mpv_, "fullscreen", MPV_FORMAT_FLAG, &flag);
  std::cout << "[DEBUG] setFullscreen: mpv_set_property result=" << result
            << std::endl;
}

void Player::seekForward(int seconds) {
  if (!mpv_)
    return;
  const char *args[] = {"seek", std::to_string(seconds).c_str(), "relative",
                        NULL};
  mpv_command_async(mpv_, 0, args);
}

void Player::seekBackward(int seconds) {
  if (!mpv_)
    return;
  const char *args[] = {"seek", std::to_string(-seconds).c_str(), "relative",
                        NULL};
  mpv_command_async(mpv_, 0, args);
}

void Player::seekTo(double position) {
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

void Player::observeProperties() {
  mpv_observe_property(mpv_, 0, "fullscreen", MPV_FORMAT_FLAG);
}

void Player::forceQuit() {
  std::cout << "[DEBUG] forceQuit called" << std::endl;
  running_ = false;
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
  if (eventThread_.joinable()) {
    eventThread_.detach();
  }
}
