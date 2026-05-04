#include "services/player/MpvCommandSender.h"
#include <array>
#include <cstdio>
#include <memory>

MpvCommandSender::MpvCommandSender(const std::string &socketPath)
    : socketPath_(socketPath) {}
void MpvCommandSender::setSocketPath(const std::string &socketPath) {
  socketPath_ = socketPath;
}

std::string MpvCommandSender::sendCommand(const std::string &jsonCmd) {
  if (socketPath_.empty())
    return "";
  std::string cmd = "echo '" + jsonCmd + "' | socat - " + socketPath_ + " 2>&1";
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
