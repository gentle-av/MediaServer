#pragma once

#include <atomic>
#include <functional>
#include <string>

class TrackLoader {
public:
  using SendCommandFunc = std::function<std::string(const std::string &)>;
  explicit TrackLoader(SendCommandFunc sendCommand);
  void loadTrack(const std::string &path, std::atomic<int> &currentIndex,
                 std::atomic<bool> &isPlaying);
  std::string escapePath(const std::string &path);

private:
  SendCommandFunc sendCommand_;
};
