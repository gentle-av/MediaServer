#include "PlaybackService.h"
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <thread>

PlaybackService::PlaybackService() : mpv(nullptr), isPlaying(false) {}

PlaybackService::~PlaybackService() {
  if (logWorkerThread.joinable()) {
    logWorkerThread.request_stop();
    logWorkerThread.join();
  }
  if (mpv) {
    mpv_terminate_destroy(mpv);
  }
  if (logFileStream.is_open()) {
    logFileStream.close();
  }
}

PlaybackService &PlaybackService::getInstance() {
  static PlaybackService instance;
  return instance;
}

void PlaybackService::setCommonOptions() {
  mpv_set_option_string(mpv, "config", "no");
  mpv_set_option_string(mpv, "really-quiet", "yes");
  mpv_set_option_string(mpv, "audio-device", "alsa");
  mpv_set_option_string(mpv, "audio-exclusive", "no");
  mpv_set_option_string(mpv, "audio-buffer", "1.0");
  mpv_set_option_string(mpv, "audio-stream-silence", "yes");
  mpv_set_option_string(mpv, "audio-format", "s16");
  mpv_set_option_string(mpv, "audio-channels", "stereo");
}

void PlaybackService::setAudioOptions() {
  std::cout << "AUDIO PLAYING!!!!\n";
  mpv_set_option_string(mpv, "vo", "null");
  mpv_set_option_string(mpv, "video", "no");
  mpv_set_option_string(mpv, "wid", "0");
  mpv_set_option_string(mpv, "fullscreen", "no");
  mpv_set_option_string(mpv, "osd-level", "0");
  mpv_set_option_string(mpv, "terminal", "no");
  mpv_set_option_string(mpv, "msg-level", "all=no");
  mpv_set_option_string(mpv, "cache", "yes");
  mpv_set_option_string(mpv, "cache-secs", "2");
  mpv_set_option_string(mpv, "demuxer-readahead-secs", "1");
  mpv_set_option_string(mpv, "no-keepaspect-window", "");
  mpv_set_option_string(mpv, "no-ontop", "");
  mpv_set_option_string(mpv, "no-border", "");
  mpv_set_option_string(mpv, "no-osc", "");
  mpv_set_option_string(mpv, "no-osd-bar", "");
  mpv_set_option_string(mpv, "no-window-dragging", "");
  mpv_set_option_string(mpv, "geometry", "0x0");
  mpv_set_option_string(mpv, "autofit", "0x0");
  mpv_set_option_string(mpv, "gpu-context", "null");
}

void PlaybackService::setVideoOptions() {
  std::cout << "VIDEO PLAYING!!!!\n";
  mpv_set_option_string(mpv, "vo", "gpu");
  mpv_set_option_string(mpv, "gpu-api", "opengl");
  mpv_set_option_string(mpv, "gpu-context", "wayland");
  mpv_set_option_string(mpv, "hwdec", "nvdec-copy");
  mpv_set_option_string(mpv, "hwdec-codecs", "hevc,h264");
  mpv_set_option_string(mpv, "scale", "bilinear");
  mpv_set_option_string(mpv, "dither", "no");
  mpv_set_option_string(mpv, "correct-downscaling", "no");
  mpv_set_option_string(mpv, "linear-downscaling", "no");
  mpv_set_option_string(mpv, "video-rotate", "0");
  mpv_set_option_string(mpv, "video-unscaled", "no");
  mpv_set_option_string(mpv, "fullscreen", "yes");
  mpv_set_option_string(mpv, "cache", "no");
  mpv_set_option_string(mpv, "cache-secs", "0");
  mpv_set_option_string(mpv, "demuxer-readahead-secs", "0");
  mpv_set_option_string(mpv, "demuxer-mkv-subtitle-preroll", "no");
  mpv_set_option_string(mpv, "demuxer-mkv-probe-start-time", "no");
  mpv_set_option_string(mpv, "vd-lavc-threads", "4");
}

void PlaybackService::configureMpv(PlaybackMode mode) {
  if (logWorkerThread.joinable()) {
    logWorkerThread.request_stop();
    logWorkerThread.join();
  }
  if (mpv) {
    mpv_terminate_destroy(mpv);
    mpv = nullptr;
  }
  mpv = mpv_create();
  if (!mpv) {
    std::cerr << "[ERROR] Failed to create mpv handle" << std::endl;
    return;
  }
  if (!logFileStream.is_open()) {
    std::filesystem::create_directories("./logs");
    logFileStream.open("./logs/mpv_core.log", std::ios::app);
  }
  if (logFileStream.is_open()) {
    mpv_request_log_messages(mpv, "v");
    logWorkerThread = std::jthread(
        [this](std::stop_token token) { this->logWorkerLoop(token); });
  }
  currentMode = mode;
  setCommonOptions();
  if (mode == PlaybackMode::AudioOnly) {
    setAudioOptions();
  } else {
    setVideoOptions();
  }
  int initResult = mpv_initialize(mpv);
  if (initResult < 0) {
    std::cerr << "[ERROR] mpv_initialize failed with code: " << initResult
              << std::endl;
    mpv_terminate_destroy(mpv);
    mpv = nullptr;
  }
}

void PlaybackService::openVideo(const std::string &path,
                                std::string &activeSocket, bool &success,
                                PlaybackMode mode) {
  std::lock_guard<std::mutex> lock(mpvMutex);
  if (mpv && currentMode != mode) {
    mpv_terminate_destroy(mpv);
    mpv = nullptr;
  }
  if (!mpv) {
    configureMpv(mode);
  }
  if (!mpv) {
    success = false;
    return;
  }
  if (isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
  }
  const char *cmd[] = {"loadfile", path.c_str(), nullptr};
  int result = mpv_command(mpv, path.c_str() ? cmd : nullptr);
  success = (result >= 0);
  if (success) {
    isPlaying = true;
    activeSocket = "libmpv-internal";
    int pause = 0;
    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    if (mode == PlaybackMode::Video) {
      int fullscreen = 1;
      mpv_set_property(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
    }
    cache.clear();
  }
}

void PlaybackService::closeVideo(std::string &activeSocket) {
  std::lock_guard<std::mutex> lock(mpvMutex);
  if (mpv && isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
    isPlaying = false;
  }
  activeSocket.clear();
  cache.clear();
}

void PlaybackService::forceStop(std::string &activeSocket) {
  std::lock_guard<std::mutex> lock(mpvMutex);
  if (mpv && isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
    isPlaying = false;
  }
  activeSocket.clear();
  cache.clear();
}

bool PlaybackService::sendCommand(const std::string &activeSocket,
                                  const std::string &command,
                                  std::string &response) {
  if (!mpv || !isPlaying)
    return false;
  int result = -1;
  std::lock_guard<std::mutex> lock(mpvMutex);
  if (command == "play" ||
      command == "{\"command\":[\"set_property\", \"pause\", false]}") {
    int pause = 0;
    result = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
  } else if (command == "pause" ||
             command == "{\"command\":[\"set_property\", \"pause\", true]}") {
    int pause = 1;
    result = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
  } else if (command.find("seek") != std::string::npos) {
    size_t start = command.find("seek") + 5;
    size_t end = command.find(",", start);
    if (end != std::string::npos) {
      double seekTime = std::stod(command.substr(start, end - start));
      if (std::isnan(seekTime) || std::isinf(seekTime) || seekTime < 0) {
        response = "{\"error\":\"invalid seek time\"}";
        return false;
      }
      const char *cmd[] = {"seek", std::to_string(seekTime).c_str(), "absolute",
                           nullptr};
      result = mpv_command(mpv, cmd);
    }
  } else if (command == "fullscreen" ||
             command == "{\"command\":[\"cycle\", \"fullscreen\"]}") {
    const char *cmd[] = {"cycle", "fullscreen", nullptr};
    result = mpv_command(mpv, cmd);
  } else if (command == "stop" || command == "{\"command\":[\"quit\"]}") {
    const char *cmd[] = {"stop", nullptr};
    result = mpv_command(mpv, cmd);
    isPlaying = false;
  }
  response =
      result >= 0 ? "{\"data\":\"success\"}" : "{\"error\":\"command failed\"}";
  if (result >= 0 && command.find("seek") != std::string::npos) {
    cache["time-pos"] = {"", std::chrono::steady_clock::now()};
  }
  return result >= 0;
}

bool PlaybackService::seek(const std::string &activeSocket, double seekTime,
                           std::string &response) {
  if (!mpv || !isPlaying)
    return false;
  if (std::isnan(seekTime) || std::isinf(seekTime) || seekTime < 0) {
    response = "{\"error\":\"invalid seek time\"}";
    return false;
  }
  std::lock_guard<std::mutex> lock(mpvMutex);
  const char *cmd[] = {"seek", std::to_string(seekTime).c_str(), "absolute",
                       nullptr};
  int result = mpv_command(mpv, cmd);
  response =
      result >= 0 ? "{\"data\":\"success\"}" : "{\"error\":\"seek failed\"}";
  if (result >= 0) {
    cache["time-pos"] = {"", std::chrono::steady_clock::now()};
  }
  return result >= 0;
}

bool PlaybackService::getProperty(const std::string &activeSocket,
                                  const std::string &property,
                                  std::string &value) {
  value = getCachedOrFetch(property);
  return !value.empty();
}

bool PlaybackService::checkProcessAlive(const std::string &activeSocket) {
  return mpv != nullptr && isPlaying;
}

bool PlaybackService::setAudioTrack(int streamIndex) {
  if (!mpv || !isPlaying)
    return false;
  std::lock_guard<std::mutex> lock(mpvMutex);
  int64_t aid = streamIndex;
  int result = mpv_set_property(mpv, "aid", MPV_FORMAT_INT64, &aid);
  return result >= 0;
}

void PlaybackService::logWorkerLoop(std::stop_token stopToken) {
  while (!stopToken.stop_requested()) {
    mpv_event *event = mpv_wait_event(mpv, 0.05);
    if (!event || event->event_id == MPV_EVENT_NONE) {
      continue;
    }
    if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
      auto *msg = static_cast<mpv_event_log_message *>(event->data);
      if (logFileStream.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&timeT));
        logFileStream << "[" << buf << "] [" << msg->level << "] ["
                      << msg->prefix << "] " << msg->text;
        logFileStream.flush();
      }
    }
  }
}

std::string PlaybackService::getCachedOrFetch(const std::string &property) {
  auto now = std::chrono::steady_clock::now();
  auto it = cache.find(property);
  if (it != cache.end() && (now - it->second.second) < CACHE_TTL) {
    return it->second.first;
  }
  if (!mpv || !isPlaying)
    return "";
  std::string result;
  std::lock_guard<std::mutex> lock(mpvMutex);
  if (property == "time-pos") {
    double val;
    if (mpv_get_property(mpv, property.c_str(), MPV_FORMAT_DOUBLE, &val) >= 0) {
      result = "{\"data\":" + std::to_string(val) + "}";
    }
  } else if (property == "duration") {
    double val;
    if (mpv_get_property(mpv, property.c_str(), MPV_FORMAT_DOUBLE, &val) >= 0) {
      result = "{\"data\":" + std::to_string(val) + "}";
    }
  } else if (property == "pause") {
    int val;
    if (mpv_get_property(mpv, property.c_str(), MPV_FORMAT_FLAG, &val) >= 0) {
      result = val ? "{\"data\":true}" : "{\"data\":false}";
    }
  } else if (property == "path") {
    const char *val;
    if (mpv_get_property(mpv, property.c_str(), MPV_FORMAT_STRING, &val) >= 0 &&
        val) {
      result = "{\"data\":\"" + std::string(val) + "\"}";
      mpv_free((void *)val);
    }
  } else if (property == "aid") {
    int64_t val;
    if (mpv_get_property(mpv, property.c_str(), MPV_FORMAT_INT64, &val) >= 0) {
      result = "{\"data\":" + std::to_string(val) + "}";
    }
  }
  if (!result.empty()) {
    cache[property] = {result, now};
  }
  return result;
}
