#pragma once

#include <atomic>
#include <mpv/client.h>
#include <thread>
#include <vector>

class Player {
public:
  Player();
  ~Player();

  bool start();
  void stop();
  void play();
  void pause();
  void setPlaylist(const std::vector<std::string> &tracks);
  std::vector<std::string> getPlaylist();
  mpv_handle *getMpvHandle() const { return mpv_; }

private:
  mpv_handle *mpv_;
  std::thread eventThread_;
  std::atomic<bool> running_;
  std::atomic<bool> manualStop_;
  std::vector<std::string> playlist_;
  int currentIndex_;

  void initMpv();
  void destroyMpv();
  void eventLoop();
  void loadTrack(int index);
  void loadNextTrack();
};
