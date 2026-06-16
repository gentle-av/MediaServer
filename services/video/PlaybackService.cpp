#include "PlaybackService.h"
#include <chrono>
#include <iostream>
#include <thread>

PlaybackService::PlaybackService() : mpv(nullptr), isPlaying(false) {
  mpv = mpv_create();
  mpv_set_option_string(mpv, "vo", "gpu-next");
  mpv_set_option_string(mpv, "hwdec", "auto-safe");
  mpv_set_option_string(mpv, "config", "no");
  mpv_set_option_string(mpv, "really-quiet", "yes");
  int fullscreen = 1;
  mpv_set_option(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
  mpv_initialize(mpv);
}

PlaybackService::~PlaybackService() {
  if (mpv) {
    mpv_terminate_destroy(mpv);
  }
}

PlaybackService &PlaybackService::getInstance() {
  static PlaybackService instance;
  return instance;
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
  }
  if (!result.empty()) {
    cache[property] = {result, now};
  }
  return result;
}

void PlaybackService::openVideo(const std::string &path,
                                std::string &activeSocket, bool &success) {
  std::cerr << "[DEBUG] PlaybackService::openVideo called with path: " << path
            << std::endl;
  if (isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  const char *cmd[] = {"loadfile", path.c_str(), nullptr};
  int result = mpv_command(mpv, cmd);
  success = (result >= 0);
  if (success) {
    isPlaying = true;
    activeSocket = "libmpv-internal";
    int pause = 0;
    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    cache.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void PlaybackService::closeVideo(std::string &activeSocket) {
  if (mpv && isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
    isPlaying = false;
  }
  activeSocket.clear();
  cache.clear();
}

void PlaybackService::forceStop(std::string &activeSocket) {
  if (mpv && isPlaying) {
    const char *cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);
    isPlaying = false;
  }
  activeSocket.clear();
  cache.clear();
  system("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
}

bool PlaybackService::sendCommand(const std::string &activeSocket,
                                  const std::string &command,
                                  std::string &response) {
  if (!mpv || !isPlaying)
    return false;
  int result = -1;
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
  if (result >= 0) {
    cache.clear();
  }
  return result >= 0;
}

bool PlaybackService::seek(const std::string &activeSocket, double seekTime,
                           std::string &response) {
  if (!mpv || !isPlaying)
    return false;
  const char *cmd[] = {"seek", std::to_string(seekTime).c_str(), "absolute",
                       nullptr};
  int result = mpv_command(mpv, cmd);
  response =
      result >= 0 ? "{\"data\":\"success\"}" : "{\"error\":\"seek failed\"}";
  if (result >= 0) {
    cache.erase("time-pos");
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
