#pragma once

#include <atomic>
#include <chrono>
#include <mpv/client.h>
#include <mutex>
#include <string>
#include <unordered_map>

class PlaybackService {
public:
  static PlaybackService &getInstance();
  ~PlaybackService();
  void openVideo(const std::string &path, std::string &activeSocket,
                 bool &success);
  void closeVideo(std::string &activeSocket);
  void forceStop(std::string &activeSocket);
  bool sendCommand(const std::string &activeSocket, const std::string &command,
                   std::string &response);
  bool seek(const std::string &activeSocket, double seekTime,
            std::string &response);
  bool getProperty(const std::string &activeSocket, const std::string &property,
                   std::string &value);
  bool checkProcessAlive(const std::string &activeSocket);
  bool setAudioTrack(int stream_index);
  void setSeekInProgress(bool inProgress) { seekInProgress_ = inProgress; }
  bool isSeekInProgress() const { return seekInProgress_.load(); }

private:
  PlaybackService();
  PlaybackService(const PlaybackService &) = delete;
  PlaybackService &operator=(const PlaybackService &) = delete;
  std::string getCachedOrFetch(const std::string &property);
  mpv_handle *mpv;
  bool isPlaying;
  std::unordered_map<
      std::string,
      std::pair<std::string, std::chrono::steady_clock::time_point>>
      cache;
  static constexpr auto CACHE_TTL = std::chrono::milliseconds(100);
  std::atomic<bool> seekInProgress_{false};
  std::mutex mpvMutex;
};
