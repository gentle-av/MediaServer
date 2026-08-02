#include "services/player/AutoAdvanceTracker.h"
#include <chrono>
#include <iostream>
#include <thread>

AutoAdvanceTracker::AutoAdvanceTracker(CommandSenderFunc sendCommand,
                                       LoadTrackFunc loadTrack)
    : sendCommand_(sendCommand), loadTrack_(loadTrack) {}

void AutoAdvanceTracker::start(std::atomic<bool> &stopFlag,
                               std::atomic<bool> &isPlaying,
                               std::vector<std::string> &tracks,
                               std::atomic<int> &currentIndex) {
  if (thread_ && thread_->joinable())
    return;
  running_ = true;
  thread_ = std::make_unique<std::thread>(
      [this, &stopFlag, &isPlaying, &tracks, &currentIndex]() {
        run(stopFlag, isPlaying, tracks, currentIndex);
      });
}

void AutoAdvanceTracker::stop() {
  running_ = false;
  if (thread_ && thread_->joinable())
    thread_->join();
}

bool AutoAdvanceTracker::isRunning() const {
  return thread_ && thread_->joinable();
}

void AutoAdvanceTracker::run(std::atomic<bool> &stopFlag,
                             std::atomic<bool> &isPlaying,
                             std::vector<std::string> &tracks,
                             std::atomic<int> &currentIndex) {
  bool wasPlaying = false;
  double lastTrackTime = 0;
  int finishCount = 0;
  while (!stopFlag && running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (stopFlag || currentIndex < 0 || currentIndex >= (int)tracks.size())
      continue;
    if (stopFlag)
      break;
    std::string pauseResp =
        sendCommand_(R"({"command": ["get_property", "pause"]})");
    std::string timeResp =
        sendCommand_(R"({"command": ["get_property", "time-pos"]})");
    std::string durationResp =
        sendCommand_(R"({"command": ["get_property", "duration"]})");
    bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
    double currentTime = 0;
    double duration = 0;
    size_t pos = timeResp.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = timeResp.find(":", pos);
      if (start != std::string::npos) {
        try {
          currentTime = std::stod(timeResp.substr(start + 1));
        } catch (...) {
        }
      }
    }
    pos = durationResp.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = durationResp.find(":", pos);
      if (start != std::string::npos) {
        try {
          duration = std::stod(durationResp.substr(start + 1));
        } catch (...) {
        }
      }
    }
    if (!isPaused && currentTime > 0) {
      wasPlaying = true;
      lastTrackTime = currentTime;
    }
    if (wasPlaying && !isPaused && currentTime < 0.5 && duration > 0 &&
        currentTime != lastTrackTime) {
      finishCount++;
      std::cout << "[AutoAdvance] track finished, count=" << finishCount
                << std::endl;
      if (finishCount >= 2) {
        if (currentIndex + 1 < (int)tracks.size()) {
          std::cout << "[AutoAdvance] switching to next track" << std::endl;
          loadTrack_(currentIndex + 1);
          finishCount = 0;
          wasPlaying = false;
          lastTrackTime = 0;
        } else {
          std::cout << "[AutoAdvance] end of playlist" << std::endl;
          isPlaying = false;
          currentIndex = -1;
          finishCount = 0;
          wasPlaying = false;
          break;
        }
      }
    }
    if (isPaused) {
      finishCount = 0;
      wasPlaying = false;
    }
  }
}
