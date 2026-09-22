#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <mpv/client.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

enum class PlaybackMode { Video, AudioOnly };

class PlaybackService {
public:
  static PlaybackService &getInstance();
  ~PlaybackService();
  void openVideo(const std::string &path, std::string &activeSocket,
                 bool &success, PlaybackMode mode = PlaybackMode::Video);
  void closeVideo(std::string &activeSocket);
  void forceStop(std::string &activeSocket);
  bool sendCommand(const std::string &activeSocket, const std::string &command,
                   std::string &response);
  bool seek(const std::string &activeSocket, double seekTime,
            std::string &response);
  bool getProperty(const std::string &activeSocket, const std::string &property,
                   std::string &value);
  bool checkProcessAlive(const std::string &activeSocket);
  bool setAudioTrack(int streamIndex);
  void setSeekInProgress(bool inProgress) { seekInProgress_ = inProgress; }
  bool isSeekInProgress() const { return seekInProgress_.load(); }

private:
  std::string getCachedOrFetch(const std::string &property);
  void configureMpv(PlaybackMode mode);
  void setCommonOptions();
  void setAudioOptions();
  void setVideoOptions();
  void logWorkerLoop(std::stop_token stopToken);
  mpv_handle *mpv;
  bool isPlaying;
  std::ofstream logFileStream;
  std::jthread logWorkerThread;
  PlaybackMode currentMode = PlaybackMode::Video;
  PlaybackService();
  PlaybackService(const PlaybackService &) = delete;
  PlaybackService &operator=(const PlaybackService &) = delete;
  std::unordered_map<
      std::string,
      std::pair<std::string, std::chrono::steady_clock::time_point>>
      cache;
  static constexpr auto CACHE_TTL = std::chrono::milliseconds(100);
  std::atomic<bool> seekInProgress_{false};
  std::mutex mpvMutex;
};
