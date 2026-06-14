#include "PlaybackService.h"
#include "ThreadMonitor.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>

static std::string escapeForShell(const std::string &arg) {
  std::string escaped = arg;
  size_t pos = 0;
  while ((pos = escaped.find('\\', pos)) != std::string::npos) {
    escaped.replace(pos, 1, "\\\\");
    pos += 2;
  }
  pos = 0;
  while ((pos = escaped.find('"', pos)) != std::string::npos) {
    escaped.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return "\"" + escaped + "\"";
}

static std::string escapeForSingleQuotes(const std::string &arg) {
  std::string escaped = arg;
  size_t pos = 0;
  while ((pos = escaped.find('\'', pos)) != std::string::npos) {
    escaped.replace(pos, 1, "'\\''");
    pos += 4;
  }
  return "'" + escaped + "'";
}

PlaybackService &PlaybackService::getInstance() {
  static PlaybackService instance;
  auto &monitor = ThreadMonitor::getInstance();
  monitor.registerThread("PlaybackServiceMain", std::this_thread::get_id());
  return instance;
}

void PlaybackService::openVideo(const std::string &path,
                                std::string &activeSocket, bool &success) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(),
                    "PlaybackService::openVideo - waiting for mpv startup");
  std::cerr << "[DEBUG] PlaybackService::openVideo called with path: " << path
            << std::endl;
  if (!activeSocket.empty()) {
    std::string escapedSocket = escapeForShell(activeSocket);
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          escapedSocket + " 2>/dev/null";
    std::cerr << "[DEBUG] Executing quitCmd: " << quitCmd << std::endl;
    system(quitCmd.c_str());
    activeSocket.clear();
    monitor.startWait(std::this_thread::get_id(),
                      "PlaybackService::openVideo - waiting after quit");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    monitor.endWait(std::this_thread::get_id());
  }
  std::string socketPath = "/tmp/mpv-socket-" + std::to_string(getpid()) + "-" +
                           std::to_string(socketCounter++);
  std::string escapedPath = escapeForShell(path);
  std::string cmd =
      "mpv --fs --vo=gpu-next --hwdec=auto-safe --input-ipc-server=" +
      socketPath + " " + escapedPath + " > /dev/null 2>&1 &";
  std::cerr << "[DEBUG] Executing cmd: " << cmd << std::endl;
  int result = system(cmd.c_str());
  std::cerr << "[DEBUG] Command result: " << result << std::endl;
  if (result == 0) {
    monitor.startWait(
        std::this_thread::get_id(),
        "PlaybackService::openVideo - waiting for socket initialization");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    monitor.endWait(std::this_thread::get_id());
    activeSocket = socketPath;
  }
  success = (result == 0);
  monitor.endWait(std::this_thread::get_id());
}

void PlaybackService::closeVideo(std::string &activeSocket) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(), "PlaybackService::closeVideo");
  std::cerr << "[DEBUG] PlaybackService::closeVideo called" << std::endl;
  if (!activeSocket.empty()) {
    std::string escapedSocket = escapeForShell(activeSocket);
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          escapedSocket + " 2>/dev/null";
    std::cerr << "[DEBUG] Executing quitCmd: " << quitCmd << std::endl;
    system(quitCmd.c_str());
    activeSocket.clear();
  }
  monitor.endWait(std::this_thread::get_id());
}

void PlaybackService::forceStop(std::string &activeSocket) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(), "PlaybackService::forceStop");
  std::cerr << "[DEBUG] PlaybackService::forceStop called" << std::endl;
  if (!activeSocket.empty()) {
    std::string escapedSocket = escapeForShell(activeSocket);
    std::string quitCmd = "echo '{\"command\": [\"quit\"]}' | socat - " +
                          escapedSocket + " 2>/dev/null";
    std::cerr << "[DEBUG] Executing quitCmd: " << quitCmd << std::endl;
    system(quitCmd.c_str());
    activeSocket.clear();
  }
  std::cerr << "[DEBUG] Killing all mpv processes" << std::endl;
  system("pkill -f 'mpv.*--input-ipc-server' 2>/dev/null");
  monitor.endWait(std::this_thread::get_id());
}

bool PlaybackService::sendCommand(const std::string &activeSocket,
                                  const std::string &command,
                                  std::string &response) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(),
                    "PlaybackService::sendCommand - waiting for socat");
  std::cerr << "[DEBUG] PlaybackService::sendCommand called with command: "
            << command << std::endl;
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
  std::string escapedSocket = escapeForShell(activeSocket);
  std::string escapedCommand = escapeForSingleQuotes(jsonCommand);
  std::string cmd =
      "echo " + escapedCommand + " | socat - " + escapedSocket + " 2>&1";
  std::cerr << "[DEBUG] Executing cmd: " << cmd << std::endl;
  std::array<char, 128> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    monitor.endWait(std::this_thread::get_id());
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    response += buffer.data();
  pclose(pipe);
  std::cerr << "[DEBUG] Response: " << response << std::endl;
  monitor.endWait(std::this_thread::get_id());
  return true;
}

bool PlaybackService::seek(const std::string &activeSocket, double seekTime,
                           std::string &response) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(),
                    "PlaybackService::seek - waiting for socat");
  std::cerr << "[DEBUG] PlaybackService::seek called with seekTime: "
            << seekTime << std::endl;
  std::string jsonCommand = "{\"command\":[\"seek\", " +
                            std::to_string(seekTime) + ", \"absolute\"]}";
  std::string escapedSocket = escapeForShell(activeSocket);
  std::string escapedCommand = escapeForSingleQuotes(jsonCommand);
  std::string cmd =
      "echo " + escapedCommand + " | socat - " + escapedSocket + " 2>&1";
  std::cerr << "[DEBUG] Executing cmd: " << cmd << std::endl;
  std::array<char, 128> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    monitor.endWait(std::this_thread::get_id());
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    response += buffer.data();
  pclose(pipe);
  monitor.endWait(std::this_thread::get_id());
  return true;
}

bool PlaybackService::getProperty(const std::string &activeSocket,
                                  const std::string &property,
                                  std::string &value) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(),
                    "PlaybackService::getProperty - waiting for socat");
  std::cerr << "[DEBUG] PlaybackService::getProperty called with property: "
            << property << std::endl;
  std::string jsonCommand =
      "{\"command\": [\"get_property\", \"" + property + "\"]}";
  std::string escapedSocket = escapeForShell(activeSocket);
  std::string escapedCommand = escapeForSingleQuotes(jsonCommand);
  std::string cmd =
      "echo " + escapedCommand + " | socat - " + escapedSocket + " 2>/dev/null";
  std::cerr << "[DEBUG] Executing cmd: " << cmd << std::endl;
  std::array<char, 128> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    monitor.endWait(std::this_thread::get_id());
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    value += buffer.data();
  pclose(pipe);
  monitor.endWait(std::this_thread::get_id());
  return true;
}

bool PlaybackService::checkProcessAlive(const std::string &activeSocket) {
  auto &monitor = ThreadMonitor::getInstance();
  monitor.startWait(std::this_thread::get_id(),
                    "PlaybackService::checkProcessAlive");
  std::string escapedSocket = escapeForShell(activeSocket);
  std::string checkCmd = "pgrep -f " + escapedSocket;
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(checkCmd.c_str(), "r");
  if (!pipe) {
    monitor.endWait(std::this_thread::get_id());
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    result += buffer.data();
  pclose(pipe);
  monitor.endWait(std::this_thread::get_id());
  return !result.empty();
}
