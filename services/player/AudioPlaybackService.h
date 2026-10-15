#pragma once

#include "MpvIpcClient.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

class AudioPlaybackService {
public:
  explicit AudioPlaybackService(std::unique_ptr<MpvIpcClient> ipcClient);
  ~AudioPlaybackService();
  void setPlaylist(std::vector<std::string> tracks, int initialIndex = 0);
  void loadTrackByIndex(int index);
  void togglePause(bool pause);
  void stop();
  void seek(double seconds);
  nlohmann::json getPlaybackState() const;
  nlohmann::json getTimeInfo() const;

private:
  void pollerLoop(std::stop_token stopToken);
  std::unique_ptr<MpvIpcClient> ipcClient;
  std::vector<std::string> currentTracks;
  int currentIndex{-1};
  bool isPlaying{false};
  mutable std::shared_mutex serviceMutex;
  mutable std::shared_mutex cacheMutex;
  mutable nlohmann::json cachedState;
  mutable std::chrono::steady_clock::time_point cachedStateTime;
  std::atomic<bool> pollerEnabled{false};
  std::jthread pollerThread;
};
