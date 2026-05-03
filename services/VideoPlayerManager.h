#pragma once

#include <functional>
#include <mutex>
#include <string>

class VideoPlayerManager {
public:
  static VideoPlayerManager &getInstance();

  void openVideo(const std::string &path,
                 std::function<void(bool, const std::string &)> callback);
  void closeVideo();
  void forceStop();
  void play();
  void pause();
  void stop();
  void seek(double time);
  void toggleFullscreen();

  struct PlaybackStatus {
    bool playing;
    bool paused;
    double currentTime;
    double duration;
    double progress;
    std::string currentFile;
  };
  PlaybackStatus getStatus();
  bool isActive() const { return !activeSocket_.empty(); }

  void sendCommand(const std::string &command);
  std::string getProperty(const std::string &property);

private:
  VideoPlayerManager() = default;

  std::string activeSocket_;
  std::mutex socketMutex_;
  static int socketCounter_;

  bool isProcessAlive();
  std::string executeCommand(const std::string &cmd);
};
