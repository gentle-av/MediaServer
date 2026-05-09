#include "PlaybackService.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <unistd.h>

PlaybackService &PlaybackService::getInstance() {
  static PlaybackService instance;
  return instance;
}

void PlaybackService::openVideo(const std::string &path,
                                std::string &activeSocket, bool &success) {
  if (!activeSocket.empty()) {
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          activeSocket + " 2>/dev/null";
    system(quitCmd.c_str());
    activeSocket.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  std::string socketPath = "/tmp/mpv-socket-" + std::to_string(getpid()) + "-" +
                           std::to_string(socketCounter++);
  std::string cmd =
      "mpv --fs --vo=gpu-next --hwdec=auto-safe --input-ipc-server=" +
      socketPath + " \"" + path + "\" > /dev/null 2>&1 &";
  int result = system(cmd.c_str());
  if (result == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    activeSocket = socketPath;
  }
  success = (result == 0);
}

void PlaybackService::closeVideo(std::string &activeSocket) {
  if (!activeSocket.empty()) {
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          activeSocket + " 2>/dev/null";
    system(quitCmd.c_str());
    activeSocket.clear();
  }
}

void PlaybackService::forceStop(std::string &activeSocket) {
  if (!activeSocket.empty()) {
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          activeSocket + " 2>/dev/null";
    system(quitCmd.c_str());
    activeSocket.clear();
  }
  system("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
}

bool PlaybackService::sendCommand(const std::string &activeSocket,
                                  const std::string &command,
                                  std::string &response) {
  std::string jsonCommand;
  if (command == "play")
    jsonCommand = "{\"command\": [\"set_property\", \"pause\", false]}";
  else if (command == "pause")
    jsonCommand = "{\"command\": [\"set_property\", \"pause\", true]}";
  else if (command == "cycle pause")
    jsonCommand = "{\"command\": [\"cycle\", \"pause\"]}";
  else if (command == "stop")
    jsonCommand = "{\"command\": [\"quit\"]}";
  else if (command == "fullscreen")
    jsonCommand = "{\"command\": [\"cycle\", \"fullscreen\"]}";
  else
    jsonCommand = "{\"command\": [\"" + command + "\"]}";
  std::string cmd =
      "echo '" + jsonCommand + "' | socat - " + activeSocket + " 2>&1";
  std::array<char, 128> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    response += buffer.data();
  }
  pclose(pipe);
  return true;
}

bool PlaybackService::getProperty(const std::string &activeSocket,
                                  const std::string &property,
                                  std::string &value) {
  std::string cmd = "echo '{\"command\": [\"get_property\", \"" + property +
                    "\"]}' | socat - " + activeSocket + " 2>/dev/null";
  std::array<char, 128> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    value += buffer.data();
  }
  pclose(pipe);
  return true;
}

bool PlaybackService::checkProcessAlive(const std::string &activeSocket) {
  std::string checkCmd = "pgrep -f '" + activeSocket + "'";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(checkCmd.c_str(), "r");
  if (!pipe) {
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  return !result.empty();
}
