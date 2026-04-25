#include "Player.h"
#include <cstring>
#include <iostream>

static const char *mpv_event_name(int event_id) {
  switch (event_id) {
  case MPV_EVENT_NONE:
    return "NONE";
  case MPV_EVENT_SHUTDOWN:
    return "SHUTDOWN";
  case MPV_EVENT_LOG_MESSAGE:
    return "LOG_MESSAGE";
  case MPV_EVENT_GET_PROPERTY_REPLY:
    return "GET_PROPERTY_REPLY";
  case MPV_EVENT_SET_PROPERTY_REPLY:
    return "SET_PROPERTY_REPLY";
  case MPV_EVENT_COMMAND_REPLY:
    return "COMMAND_REPLY";
  case MPV_EVENT_START_FILE:
    return "START_FILE";
  case MPV_EVENT_END_FILE:
    return "END_FILE";
  case MPV_EVENT_FILE_LOADED:
    return "FILE_LOADED";
  case MPV_EVENT_IDLE:
    return "IDLE";
  case MPV_EVENT_TICK:
    return "TICK";
  case MPV_EVENT_VIDEO_RECONFIG:
    return "VIDEO_RECONFIG";
  case MPV_EVENT_AUDIO_RECONFIG:
    return "AUDIO_RECONFIG";
  case MPV_EVENT_SEEK:
    return "SEEK";
  case MPV_EVENT_PLAYBACK_RESTART:
    return "PLAYBACK_RESTART";
  case MPV_EVENT_PROPERTY_CHANGE:
    return "PROPERTY_CHANGE";
  case MPV_EVENT_QUEUE_OVERFLOW:
    return "QUEUE_OVERFLOW";
  default:
    return "UNKNOWN";
  }
}

Player::Player() : mpv_(nullptr), running_(true) {
  mpv_ = mpv_create();
  if (!mpv_) {
    std::cerr << "[Player] Failed to create mpv handle" << std::endl;
    return;
  }
  mpv_set_option_string(mpv_, "ao", "pulse");
  mpv_set_option_string(mpv_, "cache", "yes");
  mpv_set_option_string(mpv_, "cache-secs", "10"); // Было 2
  mpv_set_option_string(mpv_, "demuxer-max-bytes", "100M");
  mpv_set_option_string(mpv_, "demuxer-max-back-bytes", "50M");
  mpv_set_option_string(mpv_, "demuxer-readahead-secs", "10");
  mpv_set_option_string(mpv_, "audio-buffer", "1.0");   // 1 секунда буфера
  mpv_set_option_string(mpv_, "audio-exclusive", "no"); // НЕ эксклюзивный!
  mpv_set_option_string(mpv_, "audio-pitch-correction", "yes");
  mpv_set_option_string(mpv_, "video", "no");
  mpv_set_option_string(mpv_, "vo", "null");
  mpv_set_option_string(mpv_, "osc", "no");
  mpv_set_option_string(mpv_, "idle", "yes");
  mpv_set_option_string(mpv_, "gapless-audio", "yes");
  mpv_set_option_string(mpv_, "keep-open", "no");
  mpv_set_option_string(mpv_, "volume", "100");
  mpv_set_option_string(mpv_, "rtc", "no");
  mpv_request_log_messages(mpv_, "info");
  mpv_set_wakeup_callback(mpv_, onMpvWakeup, this);
  int init_result = mpv_initialize(mpv_);
  if (init_result < 0) {
    std::cerr << "[Player] mpv_initialize failed: " << init_result << std::endl;
  }
  eventThread_ = std::thread(&Player::eventLoop, this);
  std::cout << "[Player] Constructor completed with optimized settings"
            << std::endl;
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
    std::cout << "[Player] Event: " << event->event_id << " ("
              << mpv_event_name(event->event_id) << ")" << std::endl;

    if (event->event_id == MPV_EVENT_END_FILE) {
      mpv_event_end_file *end_file = (mpv_event_end_file *)event->data;
      std::cout << "[Player] Track ended, reason: " << end_file->reason
                << " (1=EOF, 2=STOP, 3=QUIT, 4=ERROR)" << std::endl;
      if (onTrackEnd_) {
        std::cout << "[Player] Calling onTrackEnd_ callback" << std::endl;
        onTrackEnd_();
      }
    } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
      std::cout << "[Player] File loaded" << std::endl;
      if (onTrackLoaded_) {
        std::cout << "[Player] Calling onTrackLoaded_ callback" << std::endl;
        onTrackLoaded_();
      }
    } else if (event->event_id == MPV_EVENT_IDLE) {
      std::cout << "[Player] IDLE state - playback finished" << std::endl;
      if (onTrackEnd_ && playlistEnded_) {
        onTrackEnd_();
      }
      playlistEnded_ = true;
    } else if (event->event_id == MPV_EVENT_START_FILE) {
      std::cout << "[Player] Start file" << std::endl;
      playlistEnded_ = false;
    } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
      std::cout << "[Player] Playback restart" << std::endl;
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

void Player::playFile(const std::string &filePath) {
  executeCommand([this, filePath]() {
    std::cout << "[Player] playFile: " << filePath << std::endl;
    const char *cmd[] = {"loadfile", filePath.c_str(), "replace", NULL};
    int result = mpv_command(mpv_, cmd);
    std::cout << "[Player] playFile: mpv_command result=" << result
              << std::endl;
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
    std::cout << "[Player] play: BEFORE set_property" << std::endl;
    int pause = 0;
    int result = mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
    std::cout << "[Player] play: set_property result=" << result << std::endl;
    int checkPause = 1;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &checkPause);
    std::cout << "[Player] play: after set, pause=" << checkPause << std::endl;
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
  if (!mpv_)
    return false;
  int pause = 1;
  int result = mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  if (result < 0) {
    return false;
  }
  double timePos = 0;
  mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &timePos);
  const char *filename = mpv_get_property_string(mpv_, "filename");
  bool hasFile = (filename != nullptr && strlen(filename) > 0);
  if (!hasFile)
    return false;
  if (timePos > 0 && pause == 0)
    return true;
  if (timePos == 0 && pause == 0 && hasFile)
    return true;
  return false;
}

void Player::setOnTrackEnd(std::function<void()> callback) {
  onTrackEnd_ = callback;
}

void Player::setOnTrackLoaded(std::function<void()> callback) {
  onTrackLoaded_ = callback;
}
