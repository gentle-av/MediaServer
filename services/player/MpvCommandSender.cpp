#include "services/player/MpvCommandSender.h"
#include <array>
#include <cstdio>

static std::string escapeForSingleQuotes(const std::string &arg) {
  std::string escaped = arg;
  size_t pos = 0;
  while ((pos = escaped.find('\'', pos)) != std::string::npos) {
    escaped.replace(pos, 1, "'\\''");
    pos += 4;
  }
  return "'" + escaped + "'";
}

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

MpvCommandSender::MpvCommandSender(const std::string &socketPath)
    : socketPath_(socketPath) {}

void MpvCommandSender::setSocketPath(const std::string &socketPath) {
  socketPath_ = socketPath;
}

std::string MpvCommandSender::sendCommand(const std::string &jsonCmd) {
  if (socketPath_.empty())
    return "";
  std::string escapedSocket = escapeForShell(socketPath_);
  std::string escapedJson = escapeForSingleQuotes(jsonCmd);
  std::string cmd =
      "echo " + escapedJson + " | socat - " + escapedSocket + " 2>&1";
  std::array<char, 512> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  return result;
}
