#include "services/player/PlayerSessionManager.h"
#include <chrono>
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

PlayerSessionManager::PlayerSessionManager() = default;

PlayerSessionManager::~PlayerSessionManager() = default;

void PlayerSessionManager::launchMpv(const std::string &socketPath) {
  std::cout << "[DEBUG] launchMpv: Starting with socket: " << socketPath
            << std::endl;
  unlink(socketPath.c_str());
  std::cout << "[DEBUG] launchMpv: Unlinked existing socket" << std::endl;
  std::string escapedSocket = escapeForShell(socketPath);
  std::string cmd = "mpv --input-ipc-server=" + escapedSocket +
                    " --idle --no-video --no-audio-display" + " --ao=pipewire" +
                    " --no-terminal --really-quiet" +
                    " 2>&1 | logger -t mpv-server &";
  std::cout << "[DEBUG] launchMpv: Executing command: " << cmd << std::endl;
  int result = system(cmd.c_str());
  std::cout << "[DEBUG] launchMpv: system() returned: " << result << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (access(socketPath.c_str(), F_OK) == 0) {
    std::cout << "[DEBUG] launchMpv: Socket created successfully: "
              << socketPath << std::endl;
  } else {
    std::cerr << "[ERROR] launchMpv: Socket NOT created: " << socketPath
              << std::endl;
  }
  std::string checkCmd = "pgrep -f 'mpv.*" + escapedSocket + "' 2>/dev/null";
  FILE *pipe = popen(checkCmd.c_str(), "r");
  char buffer[128];
  std::string resultStr;
  while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
    resultStr += buffer;
  }
  pclose(pipe);
  if (!resultStr.empty()) {
    std::cout << "[DEBUG] launchMpv: Process running with PID: " << resultStr;
  } else {
    std::cerr << "[ERROR] launchMpv: Process NOT running!" << std::endl;
  }
}

void PlayerSessionManager::stopMpv(std::string &socketPath,
                                   std::vector<std::string> &tracks,
                                   int &currentIndex,
                                   std::atomic<bool> &isPlaying) {
  if (!socketPath.empty()) {
    std::string escapedSocket = escapeForShell(socketPath);
    std::string quitCmd =
        "timeout 2 sh -c 'echo {\"command\":[\"quit\"]} | socat - " +
        escapedSocket + " 2>/dev/null'";
    system(quitCmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    system(("pkill -f 'mpv.*" + escapedSocket + "' 2>/dev/null").c_str());
    system(("rm -f " + escapedSocket + " 2>/dev/null").c_str());
    socketPath.clear();
  }
  tracks.clear();
  currentIndex = -1;
  isPlaying = false;
}

bool PlayerSessionManager::isProcessAlive(const std::string &socketPath) {
  std::cout << "[DEBUG] isProcessAlive: Checking socket: " << socketPath
            << std::endl;
  if (socketPath.empty()) {
    std::cout << "[DEBUG] isProcessAlive: Socket path empty" << std::endl;
    return false;
  }
  if (access(socketPath.c_str(), F_OK) != 0) {
    std::cout << "[DEBUG] isProcessAlive: Socket file does not exist"
              << std::endl;
    return false;
  }
  std::string escapedSocket = escapeForShell(socketPath);
  std::string cmd = "pgrep -f 'mpv.*" + escapedSocket + "' 2>/dev/null";
  std::cout << "[DEBUG] isProcessAlive: Checking process: " << cmd << std::endl;
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  bool alive = !result.empty();
  std::cout << "[DEBUG] isProcessAlive: Process " << (alive ? "alive" : "dead")
            << std::endl;
  return alive;
}

std::string PlayerSessionManager::generateSocketPath() {
  return "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
         std::to_string(instanceCounter_++);
}

int PlayerSessionManager::instanceCounter_ = 0;
