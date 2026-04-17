#include "Player.h"
#include <iostream>

Player::Player() : mpv_(nullptr), running_(true) {
  mpv_ = mpv_create();
  mpv_set_option_string(mpv_, "volume", "100");
  mpv_set_option_string(mpv_, "cache", "yes");
  mpv_set_option_string(mpv_, "cache-secs", "2");
  mpv_set_option_string(mpv_, "video", "no");
  mpv_set_option_string(mpv_, "vo", "null");
  mpv_set_option_string(mpv_, "osc", "no");
  mpv_set_option_string(mpv_, "idle", "yes");
  int keepOpen = 1;
  mpv_set_option(mpv_, "keep-open", MPV_FORMAT_FLAG, &keepOpen);
  mpv_initialize(mpv_);
  mpv_set_wakeup_callback(mpv_, onMpvWakeup, this);
  eventThread_ = std::thread(&Player::eventLoop, this);
  std::cout << "[Player] Constructor" << std::endl;
}

Player::~Player() {
  std::cout << "[Player] Destructor" << std::endl;
  running_ = false;
  commandCv_.notify_all();
  if (eventThread_.joinable()) {
    eventThread_.join();
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
  }
}

void Player::onMpvWakeup(void *ctx) {
  static_cast<Player *>(ctx)->processEvents();
}

void Player::processEvents() {
  while (mpv_) {
    mpv_event *event = mpv_wait_event(mpv_, 0);
    if (event->event_id == MPV_EVENT_NONE) {
      break;
    }
    std::cout << "[Player] Event: " << event->event_id << std::endl;
    if (event->event_id == MPV_EVENT_END_FILE) {
      mpv_event_end_file *end_file = (mpv_event_end_file *)event->data;
      std::cout << "[Player] Track ended, reason: " << end_file->reason
                << std::endl;
      if (onTrackEnd_) {
        onTrackEnd_();
      }
    }
    if (event->event_id == MPV_EVENT_FILE_LOADED) {
      std::cout << "[Player] File loaded" << std::endl;
      if (onTrackLoaded_) {
        onTrackLoaded_();
      }
    }
  }
}

void Player::eventLoop() {
  while (running_) {
    std::unique_lock<std::mutex> lock(mutex_);
    commandCv_.wait(lock,
                    [this] { return !commandQueue_.empty() || !running_; });
    while (!commandQueue_.empty()) {
      auto cmd = commandQueue_.front();
      commandQueue_.pop();
      lock.unlock();
      cmd();
      lock.lock();
    }
  }
}

void Player::executeCommand(std::function<void()> cmd) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    commandQueue_.push(cmd);
  }
  commandCv_.notify_one();
}

void Player::playFile(const std::string &path) {
  executeCommand([this, path]() {
    std::cout << "[Player] playFile: " << path << std::endl;
    const char *cmd[] = {"loadfile", path.c_str(), "replace", NULL};
    mpv_command(mpv_, cmd);
    int pause = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  });
}

void Player::stop() {
  executeCommand([this]() {
    std::cout << "[Player] stop" << std::endl;
    const char *cmd[] = {"stop", NULL};
    mpv_command(mpv_, cmd);
  });
}

void Player::play() {
  executeCommand([this]() {
    std::cout << "[Player] play" << std::endl;
    int pause = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  });
}

void Player::pause() {
  executeCommand([this]() {
    std::cout << "[Player] pause" << std::endl;
    int pause = 1;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  });
}

void Player::seekTo(double position) {
  executeCommand([this, position]() {
    std::cout << "[Player] seekTo: " << position << std::endl;
    const char *cmd[] = {"seek", std::to_string(position).c_str(), "absolute",
                         NULL};
    mpv_command(mpv_, cmd);
  });
}

void Player::setVideoMode(bool enabled) {
  executeCommand([this, enabled]() {
    if (enabled) {
      mpv_set_option_string(mpv_, "video", "yes");
      mpv_set_option_string(mpv_, "vo", "gpu-next");
    } else {
      mpv_set_option_string(mpv_, "video", "no");
      mpv_set_option_string(mpv_, "vo", "null");
    }
  });
}

double Player::getCurrentTime() {
  double time = 0;
  mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &time);
  return time;
}

double Player::getDuration() {
  double duration = 0;
  mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &duration);
  return duration;
}

bool Player::isPlaying() {
  int pause = 1;
  mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  return pause == 0;
}

void Player::setOnTrackEnd(std::function<void()> callback) {
  onTrackEnd_ = callback;
}

void Player::setOnTrackLoaded(std::function<void()> callback) {
  onTrackLoaded_ = callback;
}
