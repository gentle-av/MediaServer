#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mpv/client.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class Player {
public:
  Player();
  ~Player();
  void playFile(const std::string &path);
  void stop();
  void play();
  void pause();
  void seekTo(double position);
  void setVideoMode(bool enabled);
  void setOnTrackEnd(std::function<void()> callback);
  void setOnTrackLoaded(std::function<void()> callback);
  double getCurrentTime();
  double getDuration();
  bool isPlaying();

private:
  mpv_handle *mpv_;
  std::atomic<bool> running_;
  std::thread eventThread_;
  std::function<void()> onTrackEnd_;
  std::function<void()> onTrackLoaded_;
  std::mutex mutex_;
  std::queue<std::function<void()>> commandQueue_;
  std::condition_variable commandCv_;
  static void onMpvWakeup(void *ctx);
  void processEvents();
  void eventLoop();
  void executeCommand(std::function<void()> cmd);
};
