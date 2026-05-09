#include "PlaybackStatus.h"
#include "PlaybackService.h"
#include <string>

PlaybackStatus &PlaybackStatus::getInstance() {
  static PlaybackStatus instance;
  return instance;
}

double PlaybackStatus::parsePropertyValue(const std::string &response,
                                          const std::string &propertyName) {
  size_t pos = response.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = response.find(":", pos);
    if (start != std::string::npos) {
      try {
        return std::stod(response.substr(start + 1));
      } catch (...) {
      }
    }
  }
  return 0;
}

Json::Value PlaybackStatus::getStatus(const std::string &activeSocket) {
  Json::Value response;
  auto &playbackService = PlaybackService::getInstance();
  if (activeSocket.empty()) {
    response["success"] = true;
    response["playing"] = false;
    response["reason"] = "no_active_video";
    return response;
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    response["success"] = true;
    response["playing"] = false;
    response["reason"] = "process_dead";
    return response;
  }
  std::string pauseResponse;
  playbackService.getProperty(activeSocket, "pause", pauseResponse);
  bool isPaused = pauseResponse.find("\"data\":true") != std::string::npos;
  std::string timeResponse;
  playbackService.getProperty(activeSocket, "time-pos", timeResponse);
  double currentTime = parsePropertyValue(timeResponse, "time-pos");
  std::string durationResponse;
  playbackService.getProperty(activeSocket, "duration", durationResponse);
  double duration = parsePropertyValue(durationResponse, "duration");
  std::string pathResponse;
  playbackService.getProperty(activeSocket, "path", pathResponse);
  std::string currentFile;
  size_t pos = pathResponse.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = pathResponse.find("\"", pos + 7);
    if (start != std::string::npos) {
      size_t end = pathResponse.find("\"", start + 1);
      if (end != std::string::npos) {
        currentFile = pathResponse.substr(start + 1, end - start - 1);
      }
    }
  }
  response["success"] = true;
  response["playing"] = true;
  response["paused"] = isPaused;
  response["currentTime"] = currentTime;
  response["duration"] = duration;
  response["progress"] = duration > 0 ? (currentTime / duration * 100) : 0;
  response["currentFile"] = currentFile;
  return response;
}

bool PlaybackStatus::seek(const std::string &activeSocket, double seekTime,
                          double &actualTime, double &duration) {
  auto &playbackService = PlaybackService::getInstance();
  std::string durationResponse;
  playbackService.getProperty(activeSocket, "duration", durationResponse);
  size_t pos = durationResponse.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = durationResponse.find(":", pos);
    if (start != std::string::npos) {
      try {
        duration = std::stod(durationResponse.substr(start + 1));
      } catch (...) {
      }
    }
  }
  if (duration > 0) {
    if (seekTime < 0)
      seekTime = 0;
    if (seekTime > duration)
      seekTime = duration;
  }
  actualTime = seekTime;
  std::string response;
  std::string seekCommand = "{\"command\":[\"seek\", " +
                            std::to_string(seekTime) + ", \"absolute\"]}";
  return playbackService.sendCommand(activeSocket, seekCommand, response);
}
