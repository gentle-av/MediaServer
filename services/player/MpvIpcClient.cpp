#include "MpvIpcClient.h"
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

MpvIpcClient::MpvIpcClient(const std::string &socketPath)
    : socketPath(socketPath) {}

MpvIpcClient::~MpvIpcClient() {
  if (!socketPath.empty()) {
    sendCommand(R"({"command": ["stop"]})");
    ::unlink(socketPath.c_str());
  }
}

void MpvIpcClient::startMpvIfNeeded() {
  if (!socketPath.empty()) {
    std::string response =
        sendCommand(R"({"command": ["get_property", "idle-active"]})");
    if (!response.empty()) {
      return;
    }
  }
  socketPath = "/tmp/mpv-socket-" + std::to_string(::getpid());
  int checkSock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (checkSock >= 0) {
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(checkSock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
      ::close(checkSock);
      return;
    }
    ::close(checkSock);
  }
  ::unlink(socketPath.c_str());
  pid_t pid = ::fork();
  if (pid == 0) {
    std::vector<std::string> args = {"mpv",
                                     "--input-ipc-server=" + socketPath,
                                     "--audio-device=alsa",
                                     "--audio-exclusive=yes",
                                     "--audio-stream-silence=yes",
                                     "--no-video",
                                     "--idle=yes",
                                     "--no-terminal",
                                     "--really-quiet"};
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
    ::_exit(127);
  }
  for (int i = 0; i < 30; ++i) {
    ::usleep(100000);
    int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock >= 0) {
      struct sockaddr_un addr;
      std::memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      std::strncpy(addr.sun_path, socketPath.c_str(),
                   sizeof(addr.sun_path) - 1);
      if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        ::close(sock);
        return;
      }
      ::close(sock);
    }
  }
}

std::string MpvIpcClient::sendCommand(const std::string &jsonCommand) const {
  if (socketPath.empty()) {
    return "";
  }
  int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    return "";
  }
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 150000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  struct sockaddr_un addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    ::close(sock);
    return "";
  }
  std::string cmd = jsonCommand + "\n";
  ::send(sock, cmd.c_str(), cmd.length(), 0);
  char buffer[4096] = {0};
  int bytesRead = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
  ::close(sock);
  if (bytesRead > 0) {
    return std::string(buffer, bytesRead);
  }
  return "";
}

double MpvIpcClient::parseResponseValue(const std::string &response) const {
  size_t pos = response.find("\"data\"");
  if (pos == std::string::npos) {
    return 0;
  }
  size_t start = response.find(":", pos);
  if (start == std::string::npos) {
    return 0;
  }
  try {
    std::string numStr = response.substr(start + 1);
    size_t end = numStr.find_first_of(",}\n\r");
    if (end != std::string::npos) {
      numStr = numStr.substr(0, end);
    }
    return std::stod(numStr);
  } catch (...) {
    return 0;
  }
}
