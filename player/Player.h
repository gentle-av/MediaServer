#pragma once

#include <atomic>
#include <mpv/client.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Player {
public:
  Player();
  ~Player();

  void stop();
  void play();
  void pause();
  void next();
  void previous();
  void setPlaylist(const std::vector<std::string> &tracks);
  std::vector<std::string> getPlaylist();
  int getCurrentIndex() const { return currentIndex_; }
  mpv_handle *getMpvHandle() const { return mpv_; }
  void setFullscreen(bool fullscreen);
  void seekTo(double position);
  bool isFullscreen() const;
  void setVideoMode(bool enabled);
  void forceQuit();
  bool isValid() const { return mpv_ != nullptr; }
  double getCurrentTime() const { return currentTime_.load(); }
  double getDuration() const { return duration_.load(); }
  bool isPlaying() const { return isPlaying_.load(); }

private:
  mpv_handle *mpv_;
  std::thread eventThread_;
  std::atomic<bool> running_;
  std::atomic<bool> manualStop_;
  std::atomic<bool> loading_;
  std::vector<std::string> playlist_;
  int currentIndex_;
  std::atomic<bool> fullscreen_;
  bool videoMode_;
  std::mutex mpvMutex_;
  std::atomic<bool> mpvValid_;
  std::atomic<bool> stopping_;
  std::atomic<double> currentTime_;
  std::atomic<double> duration_;
  std::atomic<bool> isPlaying_;

  void initMpv();
  void loadTrack(int index);
  void loadNextTrack();
  void eventLoop();
};
