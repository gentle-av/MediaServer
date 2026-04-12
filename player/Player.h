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
  explicit Player(bool enableVideo);
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
  void setVideoEnabled(bool enabled);
  void stopAsync();
  void setPlaylistAsync(const std::vector<std::string> &tracks);
  void playAsync();
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
  std::mutex mpvMutex_;
  void initMpv(bool enableVideo);
  void loadTrack(int index);
  void loadNextTrack();
  void eventLoop();
};
