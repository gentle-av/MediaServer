#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PlayerSessionManager {
public:
  PlayerSessionManager();
  ~PlayerSessionManager();

  void launchMpv(const std::string &socketPath);
  void stopMpv(std::string &socketPath, std::vector<std::string> &tracks,
               int &currentIndex, std::atomic<bool> &isPlaying);
  bool isProcessAlive(const std::string &socketPath);
  std::string generateSocketPath();

private:
  static int instanceCounter_;
};
