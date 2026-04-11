#include "services/PlayerService.h"
#include "player/Player.h"
#include <curl/curl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>

PlayerService::PlayerService(int port)
    : port_(port), available_(false), useInternalPlayer_(false),
      isPlaying_(false), currentTime_(0.0), duration_(0.0), currentIndex_(-1),
      internalPlayer_(nullptr), trackStartTimeValid_(false), switching_(false) {
  baseUrl_ = "http://0.0.0.0:" + std::to_string(port_);
  audioPlayer_ = std::make_shared<Player>(false);
  videoPlayer_ = std::make_shared<Player>(true);
  internalPlayer_ = audioPlayer_;
  ensureConnection();
}

void PlayerService::stopCurrentPlayer() {
  std::cout << "[DEBUG] stopCurrentPlayer called" << std::endl;
  if (audioPlayer_ && audioPlayer_->getMpvHandle()) {
    std::cout << "[DEBUG] Stopping audioPlayer" << std::endl;
    audioPlayer_->stop();
  }
  if (videoPlayer_ && videoPlayer_->getMpvHandle()) {
    std::cout << "[DEBUG] Stopping videoPlayer" << std::endl;
    videoPlayer_->stop();
  }
  isPlaying_ = false;
  currentTime_ = 0;
  trackStartTimeValid_ = false;
}

void PlayerService::playTrack(int index) {
  if (index < 0 || index >= (int)playlist_.size())
    return;
  currentIndex_ = index;
  currentTrack_ = playlist_[currentIndex_];
  currentTime_ = 0;
  duration_ = 0;
  trackStartTime_ = std::chrono::steady_clock::now();
  trackStartTimeValid_ = true;
  if (internalPlayer_) {
    std::string ext = std::filesystem::path(currentTrack_).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    bool isVideo =
        (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" ||
         ext == ".wmv" || ext == ".flv" || ext == ".webm");
    if (isVideo) {
      internalPlayer_->setVideoEnabled(true);
      internalPlayer_->setFullscreen(true);
    } else {
      internalPlayer_->setVideoEnabled(false);
    }
    internalPlayer_->stop();
    internalPlayer_->setPlaylist({currentTrack_});
    internalPlayer_->play();
  }
  isPlaying_ = true;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            std::string *response) {
  size_t totalSize = size * nmemb;
  response->append((char *)contents, totalSize);
  return totalSize;
}

PlayerService::~PlayerService() {}

void PlayerService::stopAll() {
  std::cout << "[DEBUG] PlayerService::stopAll called" << std::endl;
  if (audioPlayer_) {
    std::cout << "[DEBUG] Stopping audioPlayer_" << std::endl;
    audioPlayer_->stop();
  }
  if (videoPlayer_) {
    std::cout << "[DEBUG] Stopping videoPlayer_" << std::endl;
    videoPlayer_->stop();
  }
}

void PlayerService::setVideoEnabled(bool enabled) {
  std::cout << "[DEBUG] PlayerService::setVideoEnabled: " << enabled
            << std::endl;
  if (switching_.exchange(true)) {
    std::cout << "[DEBUG] Already switching, skip" << std::endl;
    return;
  }
  stopCurrentPlayer();
  if (enabled) {
    if (audioPlayer_) {
      audioPlayer_->stop();
      audioPlayer_->setPlaylist({});
    }
    if (videoPlayer_) {
      videoPlayer_->forceQuit();
    }
    videoPlayer_ = std::make_shared<Player>(true);
    internalPlayer_ = videoPlayer_;
    std::cout << "[DEBUG] Switched to videoPlayer_" << std::endl;
  } else {
    if (videoPlayer_) {
      videoPlayer_->stop();
      videoPlayer_->setPlaylist({});
    }
    if (audioPlayer_) {
      audioPlayer_->forceQuit();
    }
    audioPlayer_ = std::make_shared<Player>(false);
    internalPlayer_ = audioPlayer_;
    std::cout << "[DEBUG] Switched to audioPlayer_" << std::endl;
  }
  playlist_.clear();
  currentIndex_ = -1;
  currentTrack_ = "";
  switching_ = false;
}

void PlayerService::clear() {
  std::cout << "[DEBUG] PlayerService::clear called" << std::endl;
  sendRequest("/api/clear", "POST");
}

void PlayerService::ensurePlayerForCurrentTrack() {
  if (currentIndex_ < 0 || currentIndex_ >= (int)playlist_.size())
    return;
  std::string ext =
      std::filesystem::path(playlist_[currentIndex_]).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  bool isVideo =
      (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" ||
       ext == ".wmv" || ext == ".flv" || ext == ".webm");
  if (isVideo) {
    internalPlayer_ = videoPlayer_;
  } else {
    internalPlayer_ = audioPlayer_;
  }
}

double PlayerService::getElapsedTime() const {
  if (!trackStartTimeValid_) {
    return currentTime_;
  }
  if (isPlaying_) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - trackStartTime_)
                       .count();
    double result = currentTime_ + (elapsed / 1000.0);
    if (duration_ > 0 && result > duration_) {
      return duration_;
    }
    return result;
  } else {
    return currentTime_;
  }
}

Json::Value PlayerService::handleInternalPlay() {
  Json::Value result;
  result["success"] = true;
  if (internalPlayer_ && !playlist_.empty()) {
    if (currentIndex_ < 0) {
      currentIndex_ = 0;
      currentTrack_ = playlist_[currentIndex_];
      currentTime_ = 0;
      trackStartTime_ = std::chrono::steady_clock::now();
      trackStartTimeValid_ = true;
      internalPlayer_->setPlaylist({currentTrack_});
      internalPlayer_->play();
    } else {
      trackStartTime_ = std::chrono::steady_clock::now();
      trackStartTimeValid_ = true;
      internalPlayer_->play();
    }
    isPlaying_ = true;
  }
  return result;
}

Json::Value PlayerService::handleInternalPause() {
  Json::Value result;
  result["success"] = true;
  if (internalPlayer_) {
    if (isPlaying_ && trackStartTimeValid_) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - trackStartTime_)
                         .count();
      currentTime_ += (elapsed / 1000.0);
      trackStartTimeValid_ = false;
    }
    internalPlayer_->pause();
    isPlaying_ = false;
  }
  return result;
}

Json::Value PlayerService::handleInternalStop() {
  Json::Value result;
  result["success"] = true;
  if (internalPlayer_) {
    internalPlayer_->stop();
    isPlaying_ = false;
    currentTime_ = 0;
    currentIndex_ = -1;
    trackStartTimeValid_ = false;
    duration_ = 0;
    std::cout << "[DEBUG] handleInternalStop called" << std::endl;
  }
  return result;
}

Json::Value PlayerService::handleInternalNext() {
  Json::Value result;
  result["success"] = true;
  if (currentIndex_ + 1 < (int)playlist_.size()) {
    if (isPlaying_ && trackStartTimeValid_) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - trackStartTime_)
                         .count();
      currentTime_ += (elapsed / 1000.0);
      trackStartTimeValid_ = false;
    }
    playTrack(currentIndex_ + 1);
  }
  return result;
}

Json::Value PlayerService::handleInternalPrevious() {
  Json::Value result;
  result["success"] = true;
  if (currentIndex_ - 1 >= 0) {
    if (isPlaying_ && trackStartTimeValid_) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - trackStartTime_)
                         .count();
      currentTime_ += (elapsed / 1000.0);
      trackStartTimeValid_ = false;
    }
    playTrack(currentIndex_ - 1);
  }
  return result;
}

Json::Value
PlayerService::handleInternalReplacePlaylist(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  std::cout << "[DEBUG] handleInternalReplacePlaylist called" << std::endl;
  if (switching_.exchange(true)) {
    std::cout << "[DEBUG] Already switching, skip" << std::endl;
    return result;
  }
  if (audioPlayer_)
    audioPlayer_->stop();
  if (videoPlayer_)
    videoPlayer_->stop();
  if (data.isMember("tracks") && data["tracks"].isArray()) {
    playlist_.clear();
    for (const auto &track : data["tracks"]) {
      if (track.isString()) {
        std::string path = track.asString();
        if (std::filesystem::exists(path)) {
          playlist_.push_back(path);
          std::cout << "[DEBUG] Added to playlist: " << path << std::endl;
        } else {
          std::cout << "[ERROR] File not found, skipping: " << path
                    << std::endl;
        }
      }
    }
    if (!playlist_.empty()) {
      std::string ext =
          std::filesystem::path(playlist_[0]).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      bool isVideo =
          (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" ||
           ext == ".wmv" || ext == ".flv" || ext == ".webm");
      if (isVideo) {
        if (videoPlayer_) {
          videoPlayer_->forceQuit();
        }
        videoPlayer_ = std::make_shared<Player>(true);
        internalPlayer_ = videoPlayer_;
      } else {
        if (audioPlayer_) {
          audioPlayer_->forceQuit();
        }
        audioPlayer_ = std::make_shared<Player>(false);
        internalPlayer_ = audioPlayer_;
      }
      playTrack(0);
    }
  }
  switching_ = false;
  return result;
}

Json::Value PlayerService::handleInternalPlayIndex(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  if (data.isMember("index") && data["index"].isInt()) {
    int index = data["index"].asInt();
    if (index >= 0 && index < (int)playlist_.size()) {
      if (isPlaying_ && trackStartTimeValid_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - trackStartTime_)
                           .count();
        currentTime_ += (elapsed / 1000.0);
        trackStartTimeValid_ = false;
      }
      playTrack(index);
    }
  }
  return result;
}

Json::Value PlayerService::handleInternalGetCurrentTime() {
  updatePlaybackState();
  Json::Value data;
  data["currentTime"] = currentTime_;
  data["duration"] = duration_;
  return data;
}

void PlayerService::removeFromPlaylist(int index) {
  if (index < 0 || index >= (int)playlist_.size())
    return;
  playlist_.erase(playlist_.begin() + index);
  if (currentIndex_ == index) {
    if (playlist_.empty()) {
      currentIndex_ = -1;
      currentTrack_ = "";
      isPlaying_ = false;
      currentTime_ = 0;
      trackStartTimeValid_ = false;
      if (internalPlayer_) {
        internalPlayer_->stop();
      }
    } else if (currentIndex_ >= (int)playlist_.size()) {
      currentIndex_ = (int)playlist_.size() - 1;
      currentTrack_ = playlist_[currentIndex_];
      currentTime_ = 0;
      trackStartTime_ = std::chrono::steady_clock::now();
      trackStartTimeValid_ = true;
      if (internalPlayer_) {
        internalPlayer_->setPlaylist({currentTrack_});
      }
    } else {
      currentTrack_ = playlist_[currentIndex_];
      currentTime_ = 0;
      trackStartTime_ = std::chrono::steady_clock::now();
      trackStartTimeValid_ = true;
      if (internalPlayer_) {
        internalPlayer_->setPlaylist({currentTrack_});
      }
    }
  } else if (currentIndex_ > index) {
    currentIndex_--;
  }
}

void PlayerService::resetTrackStartTime() {
  trackStartTime_ = std::chrono::steady_clock::now();
  trackStartTimeValid_ = true;
}

Json::Value PlayerService::handleInternalGetPlaylist() {
  Json::Value playlist(Json::arrayValue);
  for (const auto &track : playlist_) {
    playlist.append(track);
  }
  return playlist;
}

Json::Value PlayerService::handleInternalClear() {
  playlist_.clear();
  currentIndex_ = -1;
  currentTrack_ = "";
  isPlaying_ = false;
  currentTime_ = 0;
  trackStartTimeValid_ = false;
  if (internalPlayer_) {
    internalPlayer_->stop();
  }
  return Json::Value();
}

Json::Value PlayerService::handleInternalGetPlaybackState() {
  Json::Value data;
  data["isPlaying"] = isPlaying_;
  data["currentTrack"] = currentTrack_;
  data["currentIndex"] = currentIndex_;
  data["totalTracks"] = (int)playlist_.size();
  return data;
}

Json::Value PlayerService::handleInternalGetCurrentTrack() {
  Json::Value data;
  data["track"] = currentTrack_;
  return data;
}

bool PlayerService::isAvailable() const {
  return useInternalPlayer_ ? (internalPlayer_ != nullptr) : available_;
}

void PlayerService::setUseInternalPlayer(bool use) { useInternalPlayer_ = use; }

std::shared_ptr<Player> PlayerService::getInternalPlayer() {
  return internalPlayer_;
}

void PlayerService::setInternalPlayer(std::shared_ptr<Player> player) {
  internalPlayer_ = player;
  if (internalPlayer_) {
    available_ = true;
  }
}

bool PlayerService::useInternalPlayer() const { return useInternalPlayer_; }

void PlayerService::ensureConnection() {
  if (useInternalPlayer_ && internalPlayer_) {
    available_ = true;
    return;
  }
  CURL *curl = curl_easy_init();
  if (!curl) {
    available_ = false;
    return;
  }
  std::string url = baseUrl_ + "/api/playbackState";
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);
  available_ = (res == CURLE_OK && httpCode == 200);
  if (!available_) {
    std::cout << "[PlayerService] Player not available on port " << port_
              << std::endl;
  }
}

Json::Value
PlayerService::handleInternalAddToPlaylist(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  std::cout << "[DEBUG] handleInternalAddToPlaylist called" << std::endl;
  if (data.isMember("path") && data["path"].isString()) {
    std::string path = data["path"].asString();
    std::cout << "[DEBUG] Adding to playlist: " << path << std::endl;
    if (std::filesystem::exists(path)) {
      playlist_.push_back(path);
      std::cout << "[DEBUG] Added, playlist size: " << playlist_.size()
                << std::endl;
    } else {
      std::cout << "[ERROR] File not found: " << path << std::endl;
    }
  } else {
    std::cout << "[DEBUG] No path parameter in request" << std::endl;
  }
  return result;
}

Json::Value
PlayerService::handleInternalAddAfterCurrent(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  return result;
}

Json::Value PlayerService::sendRequest(const std::string &endpoint,
                                       const std::string &method,
                                       const Json::Value &data) {
  if (useInternalPlayer_ && internalPlayer_) {
    using VoidHandlerFunc = std::function<Json::Value()>;
    using DataHandlerFunc = std::function<Json::Value(const Json::Value &)>;
    static const std::map<std::string, VoidHandlerFunc> voidHandlers = {
        {"/api/play", [this]() { return handleInternalPlay(); }},
        {"/api/pause", [this]() { return handleInternalPause(); }},
        {"/api/stop", [this]() { return handleInternalStop(); }},
        {"/api/next", [this]() { return handleInternalNext(); }},
        {"/api/previous", [this]() { return handleInternalPrevious(); }},
        {"/api/clear", [this]() { return handleInternalClear(); }},
        {"/api/getPlaylist", [this]() { return handleInternalGetPlaylist(); }},
        {"/api/playbackState",
         [this]() { return handleInternalGetPlaybackState(); }},
        {"/api/currentTrack",
         [this]() { return handleInternalGetCurrentTrack(); }},
        {"/api/currentTime",
         [this]() { return handleInternalGetCurrentTime(); }}};
    static const std::map<std::string, DataHandlerFunc> dataHandlers = {
        {"/api/replacePlaylist",
         [this](const Json::Value &d) {
           return handleInternalReplacePlaylist(d);
         }},
        {"/api/add",
         [this](const Json::Value &d) {
           return handleInternalAddToPlaylist(d);
         }},
        {"/api/addAfterCurrent",
         [this](const Json::Value &d) {
           return handleInternalAddAfterCurrent(d);
         }},
        {"/api/removeFromPlaylist",
         [this](const Json::Value &d) {
           return handleInternalRemoveFromPlaylist(d);
         }},
        {"/api/playIndex",
         [this](const Json::Value &d) { return handleInternalPlayIndex(d); }},
        {"/api/seek",
         [this](const Json::Value &d) { return handleInternalSeek(d); }}};
    auto itVoid = voidHandlers.find(endpoint);
    if (itVoid != voidHandlers.end()) {
      return itVoid->second();
    }
    auto itData = dataHandlers.find(endpoint);
    if (itData != dataHandlers.end()) {
      return itData->second(data);
    }
    Json::Value result;
    result["success"] = true;
    return result;
  }
  if (!available_) {
    Json::Value result;
    result["success"] = false;
    result["error"] = "Player not available";
    return result;
  }
  CURL *curl = curl_easy_init();
  if (!curl) {
    Json::Value result;
    result["success"] = false;
    return result;
  }
  std::string url = baseUrl_ + endpoint;
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  if (method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (!data.isNull()) {
      Json::StreamWriterBuilder writer;
      std::string body = Json::writeString(writer, data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      struct curl_slist *headers = nullptr;
      headers = curl_slist_append(headers, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
  }
  CURLcode res = curl_easy_perform(curl);
  Json::Value result;
  if (res == CURLE_OK) {
    Json::CharReaderBuilder reader;
    std::string errors;
    std::unique_ptr<Json::CharReader> jsonReader(reader.newCharReader());
    if (jsonReader->parse(response.c_str(), response.c_str() + response.size(),
                          &result, &errors)) {
      if (!result.isMember("success")) {
        result["success"] = true;
      }
    } else {
      result["success"] = false;
      result["error"] = "Failed to parse response";
    }
  } else {
    result["success"] = false;
    result["error"] = curl_easy_strerror(res);
    available_ = false;
  }
  curl_easy_cleanup(curl);
  return result;
}

void PlayerService::setPlaylist(const std::vector<std::string> &tracks) {
  Json::Value data;
  Json::Value tracksArray(Json::arrayValue);
  for (const auto &track : tracks) {
    tracksArray.append(track);
  }
  data["tracks"] = tracksArray;
  sendRequest("/api/replacePlaylist", "POST", data);
}

void PlayerService::addToPlaylist(const std::string &track) {
  Json::Value data;
  data["path"] = track;
  sendRequest("/api/add", "POST", data);
}

void PlayerService::addAfterCurrent(const std::string &track) {
  Json::Value data;
  data["path"] = track;
  sendRequest("/api/addAfterCurrent", "POST", data);
}

void PlayerService::replacePlaylistWithTrack(const std::string &track) {
  if (!std::filesystem::exists(track)) {
    std::cerr << "[ERROR] File not found: " << track << std::endl;
    return;
  }
  currentTrack_ = track;
  setPlaylist({track});
}

void PlayerService::replacePlaylist(const std::vector<std::string> &tracks) {
  if (!tracks.empty()) {
    currentTrack_ = tracks[0];
  }
  setPlaylist(tracks);
}

void PlayerService::play() { sendRequest("/api/play", "POST"); }

void PlayerService::pause() { sendRequest("/api/pause", "POST"); }

void PlayerService::stop() { sendRequest("/api/stop", "POST"); };

void PlayerService::next() { sendRequest("/api/next", "POST"); }

void PlayerService::previous() { sendRequest("/api/previous", "POST"); }

void PlayerService::playIndex(int index) {
  Json::Value data;
  data["index"] = index;
  sendRequest("/api/playIndex", "POST", data);
}

Json::Value PlayerService::getPlaylist() {
  return sendRequest("/api/getPlaylist", "GET");
}

Json::Value PlayerService::getPlaybackState() {
  return sendRequest("/api/playbackState", "GET");
}

Json::Value PlayerService::getCurrentTrack() {
  return sendRequest("/api/currentTrack", "GET");
}

Json::Value PlayerService::getCurrentTime() {
  return sendRequest("/api/currentTime", "GET");
}

Json::Value
PlayerService::handleInternalRemoveFromPlaylist(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  if (data.isMember("index") && data["index"].isInt()) {
    int index = data["index"].asInt();
    removeFromPlaylist(index);
  }
  return result;
}

void PlayerService::updatePlaybackState() {
  if (!internalPlayer_)
    return;
  mpv_handle *mpv = internalPlayer_->getMpvHandle();
  if (!mpv)
    return;
  double time = 0;
  mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &time);
  currentTime_ = time;
  double duration = 0;
  mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration);
  duration_ = duration;
  int pause = 1;
  mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
  bool wasPlaying = isPlaying_;
  isPlaying_ = (pause == 0);
  if (wasPlaying != isPlaying_) {
    std::cout << "[DEBUG] updatePlaybackState: isPlaying changed to "
              << isPlaying_ << std::endl;
  }
  std::cout << "[DEBUG] updatePlaybackState: currentTime=" << currentTime_
            << ", duration=" << duration_ << ", isPlaying=" << isPlaying_
            << std::endl;
}

Json::Value PlayerService::handleInternalSeek(const Json::Value &data) {
  Json::Value result;
  result["success"] = true;
  if (data.isMember("position") && data["position"].isDouble() &&
      internalPlayer_) {
    double position = data["position"].asDouble();
    internalPlayer_->seekTo(position);
    currentTime_ = position;
    trackStartTime_ = std::chrono::steady_clock::now();
    trackStartTimeValid_ = true;
  }
  return result;
}
