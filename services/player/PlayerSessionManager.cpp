#include "services/player/PlayerSessionManager.h"
#include <chrono>
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
  unlink(socketPath.c_str());
  std::string escapedSocket = escapeForShell(socketPath);
  std::string cmd = "mpv --input-ipc-server=" + escapedSocket +
                    " --idle --no-video --ao=alsa --no-terminal --really-quiet "
                    "> /dev/null 2>&1 &";
  system(cmd.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
  if (socketPath.empty() || access(socketPath.c_str(), F_OK) != 0)
    return false;
  std::string escapedSocket = escapeForShell(socketPath);
  std::string cmd =
      "timeout 1 pgrep -f 'mpv.*" + escapedSocket + "' 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return !result.empty();
}

std::string PlayerSessionManager::generateSocketPath() {
  return "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
         std::to_string(instanceCounter_++);
}

int PlayerSessionManager::instanceCounter_ = 0;
