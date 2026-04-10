#include "services/PlayerService.h"
#include "player/Player.h"
#include <curl/curl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            std::string *response) {
  size_t totalSize = size * nmemb;
  response->append((char *)contents, totalSize);
  return totalSize;
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
    internalPlayer_->setPlaylist(playlist_);
    internalPlayer_->play();
  }
  isPlaying_ = true;
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
  if (data.isMember("tracks") && data["tracks"].isArray()) {
    playlist_.clear();
    for (const auto &track : data["tracks"]) {
      if (track.isString()) {
        std::string path = track.asString();
        if (std::filesystem::exists(path)) {
          playlist_.push_back(path);
          std::cout << "[DEBUG] Added: " << path << std::endl;
        } else {
          std::cout << "[ERROR] File not found, skipping: " << path
                    << std::endl;
        }
      }
    }
    if (!playlist_.empty()) {
      playTrack(0);
    }
  }
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

PlayerService::PlayerService(int port)
    : port_(port), available_(false), useInternalPlayer_(false),
      isPlaying_(false), currentTime_(0.0), duration_(0.0), currentIndex_(-1),
      internalPlayer_(nullptr), trackStartTimeValid_(false) {
  baseUrl_ = "http://0.0.0.0:" + std::to_string(port_);
  ensureConnection();
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

PlayerService::~PlayerService() {}

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
         [this](const Json::Value &d) { return handleInternalPlayIndex(d); }}};
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

void PlayerService::stop() { sendRequest("/api/stop", "POST"); }

void PlayerService::next() { sendRequest("/api/next", "POST"); }

void PlayerService::previous() { sendRequest("/api/previous", "POST"); }

void PlayerService::playIndex(int index) {
  Json::Value data;
  data["index"] = index;
  sendRequest("/api/playIndex", "POST", data);
}

void PlayerService::clear() { sendRequest("/api/clear", "POST"); }

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
  isPlaying_ = (pause == 0);
}
