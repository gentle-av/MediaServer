#include "services/player/PlayerSessionManager.h"
#include <chrono>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

static bool executeCommandNoOutput(const std::vector<std::string> &args) {
  if (args.empty())
    return false;
  pid_t pid = fork();
  if (pid == -1)
    return false;
  if (pid == 0) {
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static std::string
executeCommandGetOutput(const std::vector<std::string> &args) {
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return "";
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return "";
  }
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[1]);
  char buffer[128];
  std::string result;
  ssize_t n;
  while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    result += buffer;
  }
  close(pipefd[0]);
  int status;
  waitpid(pid, &status, 0);
  return result;
}

PlayerSessionManager::PlayerSessionManager() = default;
PlayerSessionManager::~PlayerSessionManager() = default;

void PlayerSessionManager::launchMpv(const std::string &socketPath) {
  std::cout << "[DEBUG] launchMpv: Starting with socket: " << socketPath
            << std::endl;
  unlink(socketPath.c_str());
  std::cout << "[DEBUG] launchMpv: Unlinked existing socket" << std::endl;
  std::vector<std::string> args = {
      "mpv",           "--input-ipc-server=" + socketPath,
      "--idle",        "--no-video",
      "--ao=alsa",     "--no-terminal",
      "--really-quiet"};
  executeCommandNoOutput(args);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  if (access(socketPath.c_str(), F_OK) == 0) {
    std::cout << "[DEBUG] launchMpv: Socket created successfully: "
              << socketPath << std::endl;
  } else {
    std::cerr << "[ERROR] launchMpv: Socket NOT created: " << socketPath
              << std::endl;
  }
  std::vector<std::string> checkArgs = {"pgrep", "-f", "mpv.*" + socketPath};
  std::string resultStr = executeCommandGetOutput(checkArgs);
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
    std::vector<std::string> quitArgs = {"socat", "-", socketPath};
    std::string quitCmd = "{\"command\":[\"quit\"]}";
    int pipefd[2];
    if (pipe(pipefd) != -1) {
      pid_t pid = fork();
      if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        std::vector<char *> argv;
        for (const auto &arg : quitArgs) {
          argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
      } else if (pid > 0) {
        close(pipefd[0]);
        write(pipefd[1], quitCmd.c_str(), quitCmd.size());
        close(pipefd[1]);
        int status;
        waitpid(pid, &status, 0);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<std::string> pkillArgs = {"pkill", "-f", "mpv.*" + socketPath};
    executeCommandNoOutput(pkillArgs);
    std::vector<std::string> rmArgs = {"rm", "-f", socketPath};
    executeCommandNoOutput(rmArgs);
    socketPath.clear();
  }
  tracks.clear();
  currentIndex = -1;
  isPlaying = false;
}

bool PlayerSessionManager::isProcessAlive(const std::string &socketPath) {
  if (socketPath.empty()) {
    return false;
  }
  if (access(socketPath.c_str(), F_OK) != 0) {
    return false;
  }
  std::vector<std::string> args = {"pgrep", "-f", "mpv.*" + socketPath};
  std::string result = executeCommandGetOutput(args);
  return !result.empty();
}

std::string PlayerSessionManager::generateSocketPath() {
  return "/tmp/simple-mpv-" + std::to_string(getpid()) + "-" +
         std::to_string(instanceCounter_++);
}

int PlayerSessionManager::instanceCounter_ = 0;
