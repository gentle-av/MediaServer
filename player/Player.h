#pragma once

#include <atomic>
#include <mpv/client.h>
#include <thread>
#include <vector>

class Player {
public:
  Player();
  explicit Player(bool enableVideo);
  ~Player();

  bool start();
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
  void seekForward(int seconds);
  void seekBackward(int seconds);
  void seekTo(double position);
  bool isFullscreen() const;
  void forceQuit();

private:
  mpv_handle *mpv_;
  std::thread eventThread_;
  std::atomic<bool> running_;
  std::atomic<bool> manualStop_;
  std::atomic<bool> loading_;
  std::vector<std::string> playlist_;
  int currentIndex_;
  std::atomic<bool> fullscreen_;
  bool videoEnabled_;

  void initMpv(bool enableVideo);
  void destroyMpv();
  void eventLoop();
  void loadTrack(int index);
  void loadNextTrack();
  void observeProperties();
};
