#pragma once

#include <atomic>
#include <chrono>
#include <json/json.h>
#include <memory>
#include <string>
#include <vector>

class Player;

class PlayerService {
public:
  explicit PlayerService(int port = 6680);
  ~PlayerService();

  bool isAvailable() const;
  void ensureConnection();

  void play();
  void pause();
  void stop();
  void next();
  void previous();
  void setPlaylist(const std::vector<std::string> &tracks);
  void addToPlaylist(const std::string &track);
  void addAfterCurrent(const std::string &track);
  void replacePlaylistWithTrack(const std::string &track);
  void replacePlaylist(const std::vector<std::string> &tracks);
  void playIndex(int index);
  void clear();
  Json::Value getPlaylist();
  Json::Value getPlaybackState();
  Json::Value getCurrentTrack();
  Json::Value handleInternalSeek(const Json::Value &data);

  std::shared_ptr<Player> getInternalPlayer();
  void setInternalPlayer(std::shared_ptr<Player> player);
  bool useInternalPlayer() const;
  void setUseInternalPlayer(bool use);
  void removeFromPlaylist(int index);
  void setVideoEnabled(bool enabled);
  void stopAll();
  Json::Value getCurrentTime();
  Json::Value sendRequest(const std::string &endpoint,
                          const std::string &method = "POST",
                          const Json::Value &data = Json::Value());

private:
  double duration_;
  int port_;
  bool available_;
  bool useInternalPlayer_;
  bool isPlaying_;
  double currentTime_;
  int currentIndex_;
  std::string currentTrack_;
  std::string baseUrl_;
  std::shared_ptr<Player> internalPlayer_;
  std::vector<std::string> playlist_;
  std::mutex stateMutex_;
  std::chrono::steady_clock::time_point trackStartTime_;
  bool trackStartTimeValid_;

  Json::Value handleInternalPlay();
  Json::Value handleInternalPause();
  Json::Value handleInternalStop();
  Json::Value handleInternalNext();
  Json::Value handleInternalPrevious();
  Json::Value handleInternalReplacePlaylist(const Json::Value &data);
  Json::Value handleInternalGetPlaylist();
  Json::Value handleInternalClear();
  Json::Value handleInternalAddToPlaylist(const Json::Value &data);
  Json::Value handleInternalAddAfterCurrent(const Json::Value &data);
  Json::Value handleInternalPlayIndex(const Json::Value &data);
  Json::Value handleInternalGetPlaybackState();
  Json::Value handleInternalGetCurrentTrack();
  Json::Value handleInternalGetCurrentTime();
  Json::Value handleInternalRemoveFromPlaylist(const Json::Value &data);

  void playTrack(int index);
  void resetTrackStartTime();
  double getElapsedTime() const;
  void stopCurrentPlayer();
};
