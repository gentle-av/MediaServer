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
  mpv_set_option_string(mpv, "slang", "rus,ru,eng,en");
  mpv_set_option_string(mpv, "vo", "gpu");
  mpv_set_option_string(mpv, "gpu-api", "opengl");
  mpv_set_option_string(mpv, "gpu-context", "wayland");
  mpv_set_option_string(mpv, "fullscreen", "yes");
  mpv_set_option_string(mpv, "keep-open", "yes");
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
    {
      std::lock_guard<std::mutex> socketLock(socketMutex);
      activeSocket_ = activeSocket;
      currentFilePath_ = path;
    }
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
  {
    std::lock_guard<std::mutex> socketLock(socketMutex);
    activeSocket_.clear();
    currentFilePath_.clear();
  }
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
  {
    std::lock_guard<std::mutex> socketLock(socketMutex);
    activeSocket_.clear();
    currentFilePath_.clear();
  }
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
      std::string seekTimeStr = std::to_string(seekTime);
      const char *cmd[] = {"seek", seekTimeStr.c_str(), "absolute", nullptr};
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
    cache.erase("time-pos");
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
  std::string seekTimeStr = std::to_string(seekTime);
  const char *cmd[] = {"seek", seekTimeStr.c_str(), "absolute", nullptr};
  int result = mpv_command(mpv, cmd);
  response =
      result >= 0 ? "{\"data\":\"success\"}" : "{\"error\":\"seek failed\"}";
  if (result >= 0) {
    cache.erase("time-pos");
  }
  return result >= 0;
}

std::string PlaybackService::getCachedOrFetch(const std::string &property) {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mpvMutex);
  auto it = cache.find(property);
  if (it != cache.end() && (now - it->second.second) < CACHE_TTL) {
    if (property == "sid") {
      std::cerr << "[PlaybackService::getCachedOrFetch] sid CACHE HIT value='"
                << it->second.first << "'" << std::endl;
    }
    return it->second.first;
  }
  if (!mpv || !isPlaying) {
    if (property == "sid") {
      std::cerr << "[PlaybackService::getCachedOrFetch] sid early return mpv="
                << (mpv != nullptr) << " isPlaying=" << isPlaying << std::endl;
    }
    return "";
  }
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
  } else if (property == "aid") {
    int64_t val;
    int ret = mpv_get_property(mpv, property.c_str(), MPV_FORMAT_INT64, &val);
    if (ret >= 0) {
      result = "{\"data\":" + std::to_string(val) + "}";
    }
  } else if (property == "sid") {
    char *val = nullptr;
    int ret = mpv_get_property(mpv, property.c_str(), MPV_FORMAT_STRING, &val);
    std::cerr << "[PlaybackService::getCachedOrFetch] sid mpv_get_property ret="
              << ret << " val=" << (val ? val : "(null)") << std::endl;
    if (ret >= 0 && val) {
      std::string sval(val);
      mpv_free(val);
      if (sval == "no") {
        result = "{\"data\":-1}";
      } else {
        result = "{\"data\":" + sval + "}";
      }
    }
  }
  if (property == "sid") {
    std::cerr << "[PlaybackService::getCachedOrFetch] sid result='" << result
              << "'" << std::endl;
  }
  if (!result.empty()) {
    cache[property] = {result, now};
  }
  return result;
}

bool PlaybackService::getProperty(const std::string &activeSocket,
                                  const std::string &property,
                                  std::string &value) {
  value = getCachedOrFetch(property);
  if (property == "sid") {
    std::cerr << "[PlaybackService::getProperty] property=sid value='" << value
              << "' empty=" << value.empty() << std::endl;
  }
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

bool PlaybackService::setSubtitleTrack(int streamIndex) {
  if (!mpv || !isPlaying)
    return false;
  std::lock_guard<std::mutex> lock(mpvMutex);
  int64_t sid = streamIndex;
  int result = mpv_set_property(mpv, "sid", MPV_FORMAT_INT64, &sid);
  if (result >= 0) {
    cache.erase("sid");
  }
  return result >= 0;
}

std::string PlaybackService::getActiveSocket() const {
  std::lock_guard<std::mutex> lock(socketMutex);
  return activeSocket_;
}

void PlaybackService::setActiveSocket(const std::string &socket) {
  std::lock_guard<std::mutex> lock(socketMutex);
  activeSocket_ = socket;
}

std::string PlaybackService::getCurrentFilePath() const {
  std::lock_guard<std::mutex> lock(socketMutex);
  return currentFilePath_;
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
    } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
      std::cerr << "[PlaybackService] MPV_EVENT_FILE_LOADED" << std::endl;
      {
        std::lock_guard<std::mutex> lock(mpvMutex);
        dumpTrackList();
        mpv_set_property_string(mpv, "sid", "auto");
        cache.erase("sid");
      }
    }
  }
}

void PlaybackService::dumpTrackList() {
  if (!mpv) {
    std::cerr << "[dumpTrackList] mpv is null" << std::endl;
    return;
  }
  mpv_node node;
  int ret = mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node);
  std::cerr << "[dumpTrackList] mpv_get_property ret=" << ret << std::endl;
  if (ret < 0)
    return;
  if (node.format != MPV_FORMAT_NODE_ARRAY) {
    std::cerr << "[dumpTrackList] unexpected format: " << node.format
              << std::endl;
    mpv_free_node_contents(&node);
    return;
  }
  std::cerr << "[dumpTrackList] num tracks=" << node.u.list->num << std::endl;
  for (int i = 0; i < node.u.list->num; ++i) {
    mpv_node *item = &node.u.list->values[i];
    if (item->format != MPV_FORMAT_NODE_MAP) {
      continue;
    }
    std::string type, lang, title, codec, external;
    int id = -1;
    bool selected = false;
    for (int j = 0; j < item->u.list->num; ++j) {
      std::string key = item->u.list->keys[j];
      mpv_node *val = &item->u.list->values[j];
      if (key == "type" && val->format == MPV_FORMAT_STRING)
        type = val->u.string;
      if (key == "id" && val->format == MPV_FORMAT_INT64)
        id = static_cast<int>(val->u.int64);
      if (key == "lang" && val->format == MPV_FORMAT_STRING)
        lang = val->u.string;
      if (key == "title" && val->format == MPV_FORMAT_STRING)
        title = val->u.string;
      if (key == "codec" && val->format == MPV_FORMAT_STRING)
        codec = val->u.string;
      if (key == "external" && val->format == MPV_FORMAT_FLAG)
        external = val->u.flag ? "yes" : "no";
      if (key == "selected" && val->format == MPV_FORMAT_FLAG)
        selected = val->u.flag;
    }
    std::cerr << "[dumpTrackList]   [" << i << "] type=" << type << " id=" << id
              << " codec=" << codec << " lang=" << lang << " title='" << title
              << "' external=" << external << " selected=" << selected
              << std::endl;
  }
  mpv_free_node_contents(&node);
}

std::optional<PlaybackService::TrackInfo>
PlaybackService::getTrackById(int id) {
  if (!mpv)
    return std::nullopt;
  mpv_node node;
  if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
    return std::nullopt;
  }
  std::optional<TrackInfo> result;
  if (node.format == MPV_FORMAT_NODE_ARRAY) {
    for (int i = 0; i < node.u.list->num && !result; ++i) {
      mpv_node *item = &node.u.list->values[i];
      if (item->format != MPV_FORMAT_NODE_MAP)
        continue;
      TrackInfo t;
      for (int j = 0; j < item->u.list->num; ++j) {
        std::string key = item->u.list->keys[j];
        mpv_node *val = &item->u.list->values[j];
        if (key == "id" && val->format == MPV_FORMAT_INT64)
          t.id = static_cast<int>(val->u.int64);
        else if (key == "type" && val->format == MPV_FORMAT_STRING)
          t.type = val->u.string;
        else if (key == "codec" && val->format == MPV_FORMAT_STRING)
          t.codec = val->u.string;
        else if (key == "lang" && val->format == MPV_FORMAT_STRING)
          t.lang = val->u.string;
        else if (key == "title" && val->format == MPV_FORMAT_STRING)
          t.title = val->u.string;
        else if (key == "selected" && val->format == MPV_FORMAT_FLAG)
          t.selected = val->u.flag;
      }
      if (t.id == id) {
        result = t;
      }
    }
  }
  mpv_free_node_contents(&node);
  return result;
}

std::expected<std::vector<PlaybackService::TrackInfo>, std::string>
PlaybackService::getSubtitleTracksFromMpv() {
  std::vector<TrackInfo> result;
  if (!mpv) {
    return std::unexpected("mpv not initialized");
  }
  mpv_node node;
  if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
    return std::unexpected("track-list unavailable");
  }
  if (node.format == MPV_FORMAT_NODE_ARRAY) {
    for (int i = 0; i < node.u.list->num; ++i) {
      mpv_node *item = &node.u.list->values[i];
      if (item->format != MPV_FORMAT_NODE_MAP)
        continue;
      TrackInfo t;
      for (int j = 0; j < item->u.list->num; ++j) {
        std::string key = item->u.list->keys[j];
        mpv_node *val = &item->u.list->values[j];
        if (key == "id" && val->format == MPV_FORMAT_INT64)
          t.id = static_cast<int>(val->u.int64);
        else if (key == "type" && val->format == MPV_FORMAT_STRING)
          t.type = val->u.string;
        else if (key == "codec" && val->format == MPV_FORMAT_STRING)
          t.codec = val->u.string;
        else if (key == "lang" && val->format == MPV_FORMAT_STRING)
          t.lang = val->u.string;
        else if (key == "title" && val->format == MPV_FORMAT_STRING)
          t.title = val->u.string;
        else if (key == "selected" && val->format == MPV_FORMAT_FLAG)
          t.selected = val->u.flag;
      }
      if (t.type == "sub") {
        result.push_back(std::move(t));
      }
    }
  }
  mpv_free_node_contents(&node);
  return result;
}

std::optional<PlaybackService::TrackInfo>
PlaybackService::getTrackById(int id, const std::string &type) {
  if (!mpv)
    return std::nullopt;
  mpv_node node;
  if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
    return std::nullopt;
  }
  std::optional<TrackInfo> result;
  if (node.format == MPV_FORMAT_NODE_ARRAY) {
    for (int i = 0; i < node.u.list->num && !result; ++i) {
      mpv_node *item = &node.u.list->values[i];
      if (item->format != MPV_FORMAT_NODE_MAP)
        continue;
      TrackInfo t;
      for (int j = 0; j < item->u.list->num; ++j) {
        std::string key = item->u.list->keys[j];
        mpv_node *val = &item->u.list->values[j];
        if (key == "id" && val->format == MPV_FORMAT_INT64)
          t.id = static_cast<int>(val->u.int64);
        else if (key == "type" && val->format == MPV_FORMAT_STRING)
          t.type = val->u.string;
        else if (key == "codec" && val->format == MPV_FORMAT_STRING)
          t.codec = val->u.string;
        else if (key == "lang" && val->format == MPV_FORMAT_STRING)
          t.lang = val->u.string;
        else if (key == "title" && val->format == MPV_FORMAT_STRING)
          t.title = val->u.string;
        else if (key == "selected" && val->format == MPV_FORMAT_FLAG)
          t.selected = val->u.flag;
      }
      if (t.id != id)
        continue;
      if (!type.empty() && t.type != type)
        continue;
      result = t;
    }
  }
  mpv_free_node_contents(&node);
  return result;
}
