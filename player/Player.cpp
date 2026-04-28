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
  mpv_set_option_string(mpv_, "cache-secs", "30");
  mpv_set_option_string(mpv_, "cache-pause", "no");
  mpv_set_option_string(mpv_, "cache-pause-initial", "no");
  mpv_set_option_string(mpv_, "demuxer-max-bytes", "500M");
  mpv_set_option_string(mpv_, "demuxer-max-back-bytes", "200M");
  mpv_set_option_string(mpv_, "demuxer-readahead-secs", "30");
  mpv_set_option_string(mpv_, "demuxer-thread", "yes");
  mpv_set_option_string(mpv_, "audio-buffer", "2.0");
  mpv_set_option_string(mpv_, "audio-exclusive", "no");
  mpv_set_option_string(mpv_, "audio-pitch-correction", "yes");
  mpv_set_option_string(mpv_, "audio-stream-silence", "yes");
  mpv_set_option_string(mpv_, "video", "no");
  mpv_set_option_string(mpv_, "vo", "null");
  mpv_set_option_string(mpv_, "osc", "no");
  mpv_set_option_string(mpv_, "idle", "yes");
  mpv_set_option_string(mpv_, "gapless-audio", "yes");
  mpv_set_option_string(mpv_, "keep-open", "no");
  mpv_set_option_string(mpv_, "volume", "100");
  mpv_set_option_string(mpv_, "prefetch-playlist", "yes");
  mpv_request_log_messages(mpv_, "info");
  mpv_set_wakeup_callback(mpv_, onMpvWakeup, this);
  int init_result = mpv_initialize(mpv_);
  if (init_result < 0) {
    std::cerr << "[Player] mpv_initialize failed: " << init_result << std::endl;
  }
  eventThread_ = std::thread(&Player::eventLoop, this);
}

Player::~Player() {
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
    if (event->event_id == MPV_EVENT_END_FILE) {
      mpv_event_end_file *end_file = (mpv_event_end_file *)event->data;
      if (onTrackEnd_) {
        onTrackEnd_();
      }
    } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
      if (onTrackLoaded_) {
        onTrackLoaded_();
      }
    } else if (event->event_id == MPV_EVENT_IDLE) {
      if (onTrackEnd_ && playlistEnded_) {
        onTrackEnd_();
      }
      playlistEnded_ = true;
    } else if (event->event_id == MPV_EVENT_START_FILE) {
      playlistEnded_ = false;
    }
  }
}

void Player::eventLoop() {
  while (running_) {
    std::unique_lock<std::mutex> lock(mutex_);
    commandCv_.wait_for(lock, std::chrono::milliseconds(10),
                        [this] { return !commandQueue_.empty() || !running_; });
    std::queue<std::function<void()>> commands;
    while (!commandQueue_.empty()) {
      commands.push(std::move(commandQueue_.front()));
      commandQueue_.pop();
    }
    lock.unlock();
    while (!commands.empty()) {
      commands.front()();
      commands.pop();
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
    const char *cmd[] = {"loadfile", filePath.c_str(), "replace", NULL};
    mpv_command(mpv_, cmd);
  });
}

void Player::stop() {
  executeCommand([this]() {
    const char *cmd[] = {"stop", NULL};
    mpv_command(mpv_, cmd);
  });
}

void Player::play() {
  executeCommand([this]() {
    int pause = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  });
}

void Player::pause() {
  executeCommand([this]() {
    int pause = 1;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
  });
}

void Player::seekTo(double position) {
  executeCommand([this, position]() {
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

double Player::getAudioBufferFill() {
  if (!mpv_)
    return 0.0;
  double fill = 0.0;
  mpv_get_property(mpv_, "audio-buffer", MPV_FORMAT_DOUBLE, &fill);
  return fill;
}

bool Player::isPlaying() {
  if (!mpv_)
    return false;
  int pause = 1;
  mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &pause);
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
