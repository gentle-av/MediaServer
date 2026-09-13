#pragma once

#include <atomic>
#include <functional>
#include <mpv/client.h>
#include <string>
#include <vector>

class AutoAdvanceTracker {
public:
  using CommandSenderFunc = std::function<std::string(const std::string &)>;
  using LoadTrackFunc = std::function<void(int)>;

  AutoAdvanceTracker(CommandSenderFunc, LoadTrackFunc) {}
  ~AutoAdvanceTracker() = default;

  void start(std::atomic<bool> &, std::atomic<bool> &,
             std::vector<std::string> &, std::atomic<int> &, mpv_handle *);
  void stop();
  bool isRunning() const;
};
