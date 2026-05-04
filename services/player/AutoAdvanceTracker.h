#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class AutoAdvanceTracker {
public:
  using CommandSenderFunc = std::function<std::string(const std::string &)>;
  using LoadTrackFunc = std::function<void(int)>;
  AutoAdvanceTracker(CommandSenderFunc sendCommand, LoadTrackFunc loadTrack);
  void start(std::atomic<bool> &stopFlag, std::atomic<bool> &isPlaying,
             std::vector<std::string> &tracks, std::atomic<int> &currentIndex);
  void stop();
  bool isRunning() const;
  std::thread *getThread() { return thread_.get(); }

private:
  void run(std::atomic<bool> &stopFlag, std::atomic<bool> &isPlaying,
           std::vector<std::string> &tracks, std::atomic<int> &currentIndex);
  std::unique_ptr<std::thread> thread_;
  CommandSenderFunc sendCommand_;
  LoadTrackFunc loadTrack_;
  std::atomic<bool> running_{false};
};
