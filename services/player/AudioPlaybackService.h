#pragma once

#include "MpvIpcClient.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <vector>

class AudioPlaybackService {
public:
  explicit AudioPlaybackService(std::unique_ptr<MpvIpcClient> ipcClient);

  void setPlaylist(std::vector<std::string> tracks);
  void loadTrackByIndex(int index);
  void togglePause(bool pause);
  void stop();
  void seek(double seconds);

  nlohmann::json getPlaybackState() const;
  nlohmann::json getTimeInfo() const;

private:
  std::unique_ptr<MpvIpcClient> ipcClient;
  std::vector<std::string> currentTracks;
  int currentIndex{-1};
  bool isPlaying{false};
  mutable std::shared_mutex serviceMutex;
};
