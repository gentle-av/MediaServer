#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PlayerManager {
public:
  static PlayerManager &getInstance();

  void play();
  void pause();
  void stop();
  void next();
  void previous();
  void setPlaylist(const std::vector<std::string> &tracks);
  void addToPlaylist(const std::string &track);
  void clearPlaylist();
  void playFile(const std::string &path);
  void playIndex(int index);
  void seek(double position);
  void forceStop();

  std::vector<std::string> getPlaylist() const;
  int getCurrentIndex() const;
  bool isPlaying() const;
  bool isAlive() const;

  struct PlaybackState {
    bool isPlaying;
    std::string currentTrack;
    int currentIndex;
    int totalTracks;
    double currentTime;
    double duration;
  };
  PlaybackState getPlaybackState();
  double getCurrentTime();

  void setVolume(int volume);
  int getVolume();
  void increaseVolume();
  void decreaseVolume();
  void toggleMute();

  void switchToSpeakers();
  void switchToHeadphones();
  std::string getCurrentOutput();

private:
  PlayerManager() = default;
  ~PlayerManager();
  PlayerManager(const PlayerManager &) = delete;

  void launchMpv();
  void stopMpv();
  std::string sendCommand(const std::string &jsonCmd);
  void loadTrack(int index);
  std::string escapePath(const std::string &path);
  void startAutoAdvance();
  void resetIdleTimer();
  void scheduleStop();
  void startMpvIfNeeded();
  bool isProcessAlive();

  std::string socketPath_;
  std::vector<std::string> playlist_;
  int currentIndex_ = -1;
  static int instanceCounter_;

  std::unique_ptr<std::thread> autoAdvanceThread_;
  std::unique_ptr<std::thread> idleTimerThread_;
  std::atomic<bool> stopAutoAdvance_{false};
  std::atomic<bool> isPlaying_{false};
  std::mutex timerMutex_;
  mutable std::mutex playlistMutex_;
};
