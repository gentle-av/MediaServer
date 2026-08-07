#include "services/player/MpvCommandSender.h"
#include <array>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static bool executeCommand(const std::vector<std::string> &args,
                           std::string &output) {
  if (args.empty())
    return false;
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
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
  char buffer[512];
  ssize_t n;
  while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    output += buffer;
  }
  close(pipefd[0]);
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

MpvCommandSender::MpvCommandSender(const std::string &socketPath)
    : socketPath_(socketPath) {}

void MpvCommandSender::setSocketPath(const std::string &socketPath) {
  socketPath_ = socketPath;
}

std::string MpvCommandSender::sendCommand(const std::string &jsonCmd) {
  if (socketPath_.empty())
    return "";
  std::vector<std::string> args = {"socat", "-", socketPath_};
  std::string output;
  if (!executeCommand(args, output)) {
    return "";
  }
  std::string result;
  size_t pos = 0;
  while (pos < output.length()) {
    size_t end = output.find('\n', pos);
    if (end == std::string::npos)
      break;
    std::string line = output.substr(pos, end - pos);
    if (!line.empty()) {
      result = line;
      break;
    }
    pos = end + 1;
  }
  return result;
}
