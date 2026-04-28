#pragma once

#include <atomic>
#include <database/MusicDatabase.h>
#include <json/json.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Player;

class PlayerService {
public:
  PlayerService(int port = 6680);
  ~PlayerService();
  void play();
  void pause();
  void stop();
  void next();
  void previous();
  void setPlaylist(const std::vector<std::string> &tracks);
  void addToPlaylist(const std::string &track);
  void addAfterCurrent(const std::string &track);
  void replacePlaylist(const std::vector<std::string> &tracks);
  void replacePlaylistWithTrack(const std::string &track);
  void playIndex(int index);
  void clear();
  void removeFromPlaylist(int index);
  Json::Value getPlaylist();
  Json::Value getPlaybackState();
  Json::Value getCurrentTrack();
  Json::Value getCurrentTime();
  void setMusicDatabase(std::shared_ptr<MusicDatabase> db) { musicDb_ = db; }
  void setInternalPlayer(std::shared_ptr<Player> player);
  bool isAvailable() const { return internalPlayer_ != nullptr; }
  bool useInternalPlayer() const { return internalPlayer_ != nullptr; }
  std::shared_ptr<Player> getInternalPlayer() { return internalPlayer_; }
  std::string getTrackTitle(const std::string &trackPath);

private:
  int currentIndex_;
  std::vector<std::string> playlist_;
  std::shared_ptr<Player> internalPlayer_;
  std::mutex mutex_;
  std::atomic<bool> isSwitching_;
  std::atomic<bool> manualSwitch_;
  std::shared_ptr<MusicDatabase> musicDb_;
  void onTrackEnd();
  void onTrackLoaded();
  void playTrack(int index);
};
