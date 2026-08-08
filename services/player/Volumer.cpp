#include "services/player/Volumer.h"
#include <regex>
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

static int executeCommandGetOutput(const std::vector<std::string> &args) {
  std::string output;
  if (!executeCommand(args, output))
    return -1;
  std::regex volumeRegex(R"((\d+)%)");
  std::smatch match;
  if (std::regex_search(output, match, volumeRegex)) {
    try {
      return std::stoi(match[1].str());
    } catch (...) {
    }
  }
  return -1;
}

static bool executeCommandNoOutput(const std::vector<std::string> &args) {
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
    close(pipefd[1]);
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[0]);
  close(pipefd[1]);
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool Volumer::isSoundAvailable() const {
  return access("/dev/snd/controlC0", R_OK) == 0;
}

int Volumer::getVolume() const {
  if (!isSoundAvailable())
    return 50;
  std::vector<std::string> args = {"timeout", "1", "amixer", "get", "Master"};
  return executeCommandGetOutput(args);
}

bool Volumer::setVolume(int volume) {
  if (!isSoundAvailable())
    return false;
  if (volume < 0 || volume > 100)
    return false;
  int amixerValue = MIN_AMIXER + (volume * (MAX_AMIXER - MIN_AMIXER) / 100);
  std::vector<std::string> args = {"amixer", "set", "Master",
                                   std::to_string(amixerValue)};
  return executeCommandNoOutput(args);
}

void Volumer::increaseVolume() {
  if (!isSoundAvailable())
    return;
  std::vector<std::string> args = {"amixer", "set", "Master", "5%+"};
  executeCommandNoOutput(args);
}

void Volumer::decreaseVolume() {
  if (!isSoundAvailable())
    return;
  std::vector<std::string> args = {"amixer", "set", "Master", "5%-"};
  executeCommandNoOutput(args);
}

void Volumer::toggleMute() {
  if (!isSoundAvailable())
    return;
  std::vector<std::string> args = {"amixer", "set", "Master", "toggle"};
  executeCommandNoOutput(args);
}
