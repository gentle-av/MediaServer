#pragma once

#include <string>

class MpvIpcClient {
public:
  explicit MpvIpcClient(const std::string &socketPath);
  ~MpvIpcClient();

  void startMpvIfNeeded();
  std::string sendCommand(const std::string &jsonCommand) const;
  double parseResponseValue(const std::string &response) const;

private:
  std::string socketPath;
};
