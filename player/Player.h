#pragma once
#include <atomic>
#include <mpv/client.h>
#include <mutex>
#include <thread>
#include <vector>

class Musium {
public:
  Musium();
  ~Musium();

  bool start();
  void stop();
  void play();
  void pause();
  void setPlaylist(const std::vector<std::string> &tracks);
  std::vector<std::string> getPlaylist();

private:
  mpv_handle *mpv_;
  std::thread eventThread_;
  std::atomic<bool> running_;
  std::vector<std::string> playlist_;
  int currentIndex_;

  void initMpv();
  void destroyMpv();
  void eventLoop();
  void loadTrack(int index);
  void loadNextTrack();
};
