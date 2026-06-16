#include "VideoControlHandler.h"
#include "FileSystemService.h"
#include "PlaybackService.h"
#include <atomic>
#include <mutex>

VideoControlHandler &VideoControlHandler::getInstance() {
  static VideoControlHandler instance;
  return instance;
}

Json::Value VideoControlHandler::handleOpen(const std::string &path,
                                            std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  auto &fsService = FileSystemService::getInstance();
  static std::mutex videoMutex;
  static std::atomic<bool> isOpening{false};
  Json::Value response;
  if (!fsService.isPathAllowed(path)) {
    response["success"] = false;
    response["error"] = "Access denied";
    return response;
  }
  if (!fsService.fileExists(path)) {
    response["success"] = false;
    response["error"] = "File not found";
    return response;
  }
  std::lock_guard<std::mutex> lock(videoMutex);
  if (isOpening) {
    response["success"] = false;
    response["error"] = "Video opening already in progress";
    return response;
  }
  isOpening = true;
  bool success = false;
  playbackService.openVideo(path, activeSocket, success);
  isOpening = false;
  response["success"] = success;
  response["socket"] = activeSocket;
  response["message"] = success ? "Video playing" : "Failed to start mpv";
  return response;
}

Json::Value VideoControlHandler::handleClose(std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  Json::Value response;
  if (!activeSocket.empty()) {
    playbackService.closeVideo(activeSocket);
    response["success"] = true;
    response["message"] = "Video closed and socket cleared";
  } else {
    response["success"] = true;
    response["message"] = "No active video to close";
  }
  return response;
}

Json::Value VideoControlHandler::handleForceStop(std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  playbackService.forceStop(activeSocket);
  Json::Value response;
  response["success"] = true;
  response["message"] = "Video force stopped";
  return response;
}

Json::Value VideoControlHandler::handleControl(const std::string &command,
                                               std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  Json::Value response;
  if (activeSocket.empty()) {
    response["success"] = false;
    response["error"] = "No active video playing";
    return response;
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    activeSocket.clear();
    response["success"] = false;
    response["error"] = "MPV process is dead";
    return response;
  }
  std::string socatResponse;
  playbackService.sendCommand(activeSocket, command, socatResponse);
  response["success"] = true;
  response["command_sent"] = command;
  response["socat_response"] = socatResponse;
  if (command == "stop" || command == "{\"command\":[\"quit\"]}") {
    activeSocket.clear();
  }
  return response;
}

Json::Value VideoControlHandler::handleSeek(double seekTime,
                                            std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  Json::Value response;
  if (activeSocket.empty()) {
    response["success"] = false;
    response["error"] = "No active video playing";
    return response;
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    activeSocket.clear();
    response["success"] = false;
    response["error"] = "MPV process is dead";
    return response;
  }
  std::string seekResponse;
  bool result = playbackService.seek(activeSocket, seekTime, seekResponse);
  response["success"] = result;
  response["time"] = seekTime;
  return response;
}

Json::Value
VideoControlHandler::handleGetProperty(const std::string &propertyName,
                                       const std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  Json::Value response;
  if (activeSocket.empty()) {
    response["success"] = false;
    response["error"] = "No active video playing";
    return response;
  }
  std::string value;
  playbackService.getProperty(activeSocket, propertyName, value);
  response["success"] = true;
  response["property"] = propertyName;
  response["value"] = value;
  return response;
}
