#pragma once

#include <string>

class MpvCommandSender {
public:
  explicit MpvCommandSender(const std::string &socketPath);
  std::string sendCommand(const std::string &jsonCmd);
  void setSocketPath(const std::string &socketPath);

private:
  std::string socketPath_;
};
