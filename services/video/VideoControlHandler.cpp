#include "VideoControlHandler.h"
#include "FileSystemService.h"
#include "PlaybackService.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

VideoControlHandler &VideoControlHandler::getInstance() {
  static VideoControlHandler instance;
  return instance;
}

Json::Value VideoControlHandler::handleOpen(const std::string &path,
                                            std::string &activeSocket,
                                            bool audioOnly) {
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
  PlaybackMode mode = audioOnly ? PlaybackMode::AudioOnly : PlaybackMode::Video;
  playbackService.openVideo(path, activeSocket, success, mode);
  isOpening = false;
  response["success"] = success;
  response["socket"] = activeSocket;
  response["message"] = success
                            ? (audioOnly ? "Audio playing" : "Video playing")
                            : "Failed to start mpv";
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
  if (std::isnan(seekTime) || std::isinf(seekTime) || seekTime < 0) {
    response["success"] = false;
    response["error"] = "Invalid seek time";
    return response;
  }
  std::string seekResponse;
  bool result = playbackService.seek(activeSocket, seekTime, seekResponse);
  response["success"] = result;
  response["time"] = seekTime;
  return response;
}

void VideoControlHandler::asyncSeek(double seekTime,
                                    std::function<void(Json::Value)> callback,
                                    std::string &activeSocket) {
  if (isSeeking) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Seek already in progress";
    callback(response);
    return;
  }
  if (std::isnan(seekTime) || std::isinf(seekTime) || seekTime < 0) {
    Json::Value response;
    response["success"] = false;
    response["error"] = "Invalid seek time";
    callback(response);
    return;
  }
  isSeeking = true;
  std::thread([this, seekTime, callback, activeSocket]() {
    auto &playbackService = PlaybackService::getInstance();
    Json::Value response;
    if (!playbackService.checkProcessAlive(activeSocket)) {
      response["success"] = false;
      response["error"] = "MPV process is dead";
      isSeeking = false;
      callback(response);
      return;
    }
    std::string seekResponse;
    bool result = playbackService.seek(activeSocket, seekTime, seekResponse);
    response["success"] = result;
    response["time"] = seekTime;
    isSeeking = false;
    callback(response);
  }).detach();
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
  if (!value.empty()) {
    Json::Value jsonValue;
    Json::Reader reader;
    if (reader.parse(value, jsonValue)) {
      if (jsonValue.isMember("data")) {
        response["value"] = jsonValue["data"];
      } else {
        response["value"] = jsonValue;
      }
    } else {
      try {
        if (propertyName == "audio" || propertyName == "aid") {
          int intValue = std::stoi(value);
          response["value"] = intValue;
        } else {
          response["value"] = value;
        }
      } catch (...) {
        response["value"] = value;
      }
    }
  } else {
    response["value"] = Json::Value();
  }
  return response;
}

Json::Value
VideoControlHandler::handleSetAudioTrack(int streamIndex,
                                         const std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  Json::Value response;
  if (activeSocket.empty()) {
    response["success"] = false;
    response["error"] = "No active video playing";
    return response;
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    response["success"] = false;
    response["error"] = "MPV process is dead";
    return response;
  }
  bool result = playbackService.setAudioTrack(streamIndex);
  response["success"] = result;
  response["streamIndex"] = streamIndex;
  return response;
}
