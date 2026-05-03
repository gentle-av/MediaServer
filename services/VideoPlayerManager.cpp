#include "services/VideoPlayerManager.h"
#include <array>
#include <chrono>
#include <thread>
#include <unistd.h>

int VideoPlayerManager::socketCounter_ = 0;

VideoPlayerManager &VideoPlayerManager::getInstance() {
  static VideoPlayerManager instance;
  return instance;
}

bool VideoPlayerManager::isProcessAlive() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (activeSocket_.empty())
    return false;
  std::string checkCmd = "pgrep -f '" + activeSocket_ + "' 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(checkCmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return !result.empty();
}

std::string VideoPlayerManager::executeCommand(const std::string &cmd) {
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return result;
}

void VideoPlayerManager::openVideo(
    const std::string &path,
    std::function<void(bool, const std::string &)> callback) {
  std::thread([this, path, callback]() {
    {
      std::lock_guard<std::mutex> lock(socketMutex_);
      if (!activeSocket_.empty()) {
        executeCommand("echo '{\"command\": [\"quit\"]}' | socat - " +
                       activeSocket_ + " 2>/dev/null");
        activeSocket_.clear();
      }
    }
    executeCommand("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string socketPath = "/tmp/mpv-socket-" + std::to_string(getpid()) +
                             "-" + std::to_string(socketCounter_++);
    std::string cmd =
        "mpv --fs --vo=gpu-next --hwdec=auto-safe --input-ipc-server=" +
        socketPath + " \"" + path + "\" > /dev/null 2>&1 &";
    int result = system(cmd.c_str());
    if (result == 0) {
      std::lock_guard<std::mutex> lock(socketMutex_);
      activeSocket_ = socketPath;
    }
    if (callback) {
      callback(result == 0, socketPath);
    }
  }).detach();
}

void VideoPlayerManager::closeVideo() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty()) {
    executeCommand("echo '{\"command\": [\"quit\"]}' | socat - " +
                   activeSocket_ + " 2>/dev/null");
    activeSocket_.clear();
  }
}

void VideoPlayerManager::forceStop() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty()) {
    executeCommand("echo '{\"command\": [\"quit\"]}' | socat - " +
                   activeSocket_ + " 2>/dev/null");
    activeSocket_.clear();
  }
  executeCommand("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
}

void VideoPlayerManager::play() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    executeCommand("echo '{\"command\": [\"set_property\", \"pause\", false]}' "
                   "| socat - " +
                   activeSocket_ + " 2>/dev/null");
  }
}

void VideoPlayerManager::pause() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    executeCommand(
        "echo '{\"command\": [\"set_property\", \"pause\", true]}' | socat - " +
        activeSocket_ + " 2>/dev/null");
  }
}

void VideoPlayerManager::stop() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    executeCommand("echo '{\"command\": [\"stop\"]}' | socat - " +
                   activeSocket_ + " 2>/dev/null");
  }
}

void VideoPlayerManager::seek(double time) {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    executeCommand("echo '{\"command\":[\"seek\", " + std::to_string(time) +
                   ", \"absolute\"]}' | socat - " + activeSocket_ +
                   " 2>/dev/null");
  }
}

void VideoPlayerManager::toggleFullscreen() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    executeCommand(
        "echo '{\"command\": [\"cycle\", \"fullscreen\"]}' | socat - " +
        activeSocket_ + " 2>/dev/null");
  }
}

VideoPlayerManager::PlaybackStatus VideoPlayerManager::getStatus() {
  PlaybackStatus status;
  status.playing = false;
  status.paused = false;
  status.currentTime = 0;
  status.duration = 0;
  status.progress = 0;
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (activeSocket_.empty() || !isProcessAlive()) {
    return status;
  }
  std::string pauseResp = executeCommand(
      "echo '{\"command\": [\"get_property\", \"pause\"]}' | socat - " +
      activeSocket_ + " 2>/dev/null");
  status.paused = pauseResp.find("\"data\":true") != std::string::npos;
  std::string timeResp = executeCommand(
      "echo '{\"command\": [\"get_property\", \"time-pos\"]}' | socat - " +
      activeSocket_ + " 2>/dev/null");
  size_t pos = timeResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = timeResp.find(":", pos);
    if (start != std::string::npos) {
      try {
        status.currentTime = std::stod(timeResp.substr(start + 1));
      } catch (...) {
      }
    }
  }
  std::string durationResp = executeCommand(
      "echo '{\"command\": [\"get_property\", \"duration\"]}' | socat - " +
      activeSocket_ + " 2>/dev/null");
  pos = durationResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = durationResp.find(":", pos);
    if (start != std::string::npos) {
      try {
        status.duration = std::stod(durationResp.substr(start + 1));
      } catch (...) {
      }
    }
  }
  std::string pathResp = executeCommand(
      "echo '{\"command\": [\"get_property\", \"path\"]}' | socat - " +
      activeSocket_ + " 2>/dev/null");
  pos = pathResp.find("\"data\"");
  if (pos != std::string::npos) {
    size_t start = pathResp.find("\"", pos + 7);
    if (start != std::string::npos) {
      size_t end = pathResp.find("\"", start + 1);
      if (end != std::string::npos) {
        status.currentFile = pathResp.substr(start + 1, end - start - 1);
      }
    }
  }
  status.playing =
      !status.paused && (status.currentTime > 0 || status.duration > 0);
  if (status.duration > 0) {
    status.progress = (status.currentTime / status.duration) * 100;
  }

  return status;
}

void VideoPlayerManager::sendCommand(const std::string &command) {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (!activeSocket_.empty() && isProcessAlive()) {
    std::string jsonCmd;
    if (command == "play")
      jsonCmd = "{\"command\": [\"set_property\", \"pause\", false]}";
    else if (command == "pause")
      jsonCmd = "{\"command\": [\"set_property\", \"pause\", true]}";
    else if (command == "stop")
      jsonCmd = "{\"command\": [\"quit\"]}";
    else if (command == "fullscreen")
      jsonCmd = "{\"command\": [\"cycle\", \"fullscreen\"]}";
    else
      jsonCmd = "{\"command\": [\"" + command + "\"]}";
    executeCommand("echo '" + jsonCmd + "' | socat - " + activeSocket_ +
                   " 2>/dev/null");
  }
}

std::string VideoPlayerManager::getProperty(const std::string &property) {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (activeSocket_.empty() || !isProcessAlive())
    return "";
  return executeCommand("echo '{\"command\": [\"get_property\", \"" + property +
                        "\"]}' | socat - " + activeSocket_ + " 2>/dev/null");
}
