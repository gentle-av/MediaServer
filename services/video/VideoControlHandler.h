#pragma once

#include <atomic>
#include <functional>
#include <json/json.h>
#include <mutex>
#include <queue>
#include <string>

class VideoControlHandler {
public:
  static VideoControlHandler &getInstance();
  Json::Value handleOpen(const std::string &path, std::string &activeSocket,
                         bool audioOnly = false);
  Json::Value handleClose(std::string &activeSocket);
  Json::Value handleForceStop(std::string &activeSocket);
  Json::Value handleControl(const std::string &command,
                            std::string &activeSocket);
  Json::Value handleSeek(double seekTime, std::string &activeSocket);
  Json::Value handleGetProperty(const std::string &propertyName,
                                const std::string &activeSocket);
  Json::Value handleSetAudioTrack(int stream_index,
                                  const std::string &activeSocket);
  void asyncSeek(double seekTime, std::function<void(Json::Value)> callback,
                 std::string &activeSocket);

private:
  VideoControlHandler() = default;
  struct SeekCommand {
    double time;
    std::function<void(Json::Value)> callback;
    std::string socket;
  };
  std::atomic<bool> isSeeking_{false};
  std::mutex seekMutex_;
  std::queue<SeekCommand> pendingSeeks_;
};
