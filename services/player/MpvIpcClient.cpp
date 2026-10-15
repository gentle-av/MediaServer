#include "MpvIpcClient.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
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
  if (!socketPath.empty() && ::access(socketPath.c_str(), F_OK) == 0) {
    int probeSock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probeSock >= 0) {
      struct sockaddr_un probeAddr;
      std::memset(&probeAddr, 0, sizeof(probeAddr));
      probeAddr.sun_family = AF_UNIX;
      std::strncpy(probeAddr.sun_path, socketPath.c_str(),
                   sizeof(probeAddr.sun_path) - 1);
      int probeFlags = ::fcntl(probeSock, F_GETFL, 0);
      ::fcntl(probeSock, F_SETFL, probeFlags | O_NONBLOCK);
      int probeResult = ::connect(probeSock, (struct sockaddr *)&probeAddr,
                                  sizeof(probeAddr));
      if (probeResult == 0 || errno == EINPROGRESS) {
        struct pollfd probePfd{probeSock, POLLOUT, 0};
        if (::poll(&probePfd, 1, 50) > 0) {
          int probeErr = 0;
          socklen_t probeLen = sizeof(probeErr);
          ::getsockopt(probeSock, SOL_SOCKET, SO_ERROR, &probeErr, &probeLen);
          if (probeErr == 0) {
            ::close(probeSock);
            return;
          }
        }
      }
      ::close(probeSock);
    }
  }
  socketPath = "/tmp/mpv-socket-" + std::to_string(::getpid());
  ::unlink(socketPath.c_str());
  pid_t pid = ::fork();
  if (pid == 0) {
    std::vector<std::string> args = {"mpv",
                                     "--input-ipc-server=" + socketPath,
                                     "--audio-device=alsa",
                                     "--audio-stream-silence=yes",
                                     "--no-video",
                                     "--idle=yes",
                                     "--keep-open=yes",
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
    ::usleep(50000);
    if (::access(socketPath.c_str(), F_OK) != 0) {
      continue;
    }
    int probeSock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probeSock < 0) {
      continue;
    }
    struct sockaddr_un probeAddr;
    std::memset(&probeAddr, 0, sizeof(probeAddr));
    probeAddr.sun_family = AF_UNIX;
    std::strncpy(probeAddr.sun_path, socketPath.c_str(),
                 sizeof(probeAddr.sun_path) - 1);
    int probeFlags = ::fcntl(probeSock, F_GETFL, 0);
    ::fcntl(probeSock, F_SETFL, probeFlags | O_NONBLOCK);
    int probeResult =
        ::connect(probeSock, (struct sockaddr *)&probeAddr, sizeof(probeAddr));
    if (probeResult == 0) {
      ::close(probeSock);
      return;
    }
    if (errno == EINPROGRESS) {
      struct pollfd probePfd{probeSock, POLLOUT, 0};
      if (::poll(&probePfd, 1, 50) > 0) {
        int probeErr = 0;
        socklen_t probeLen = sizeof(probeErr);
        ::getsockopt(probeSock, SOL_SOCKET, SO_ERROR, &probeErr, &probeLen);
        if (probeErr == 0) {
          ::close(probeSock);
          return;
        }
      }
    }
    ::close(probeSock);
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
  int flags = ::fcntl(sock, F_GETFL, 0);
  ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  int connResult = ::connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (connResult < 0 && errno != EINPROGRESS) {
    ::close(sock);
    return "";
  }
  if (connResult < 0) {
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);
    int selResult = ::select(sock + 1, nullptr, &writeSet, nullptr, &tv);
    if (selResult <= 0) {
      ::close(sock);
      return "";
    }
    int sockErr = 0;
    socklen_t len = sizeof(sockErr);
    if (::getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0 ||
        sockErr != 0) {
      ::close(sock);
      return "";
    }
  }
  std::string cmd = jsonCommand + "\n";
  ssize_t sent = ::send(sock, cmd.c_str(), cmd.length(), MSG_NOSIGNAL);
  if (sent <= 0) {
    ::close(sock);
    return "";
  }
  std::string response;
  char buffer[4096] = {0};
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
  bool gotNewline = false;
  while (!gotNewline) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      break;
    }
    int remainingMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count());
    if (remainingMs <= 0) {
      break;
    }
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pollResult = ::poll(&pfd, 1, remainingMs);
    if (pollResult <= 0) {
      break;
    }
    if (!(pfd.revents & POLLIN)) {
      break;
    }
    ssize_t bytesRead = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
      break;
    }
    response.append(buffer, bytesRead);
    if (response.find('\n') != std::string::npos) {
      gotNewline = true;
    }
  }
  ::close(sock);
  return response;
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
