#include "AutoAdvanceTracker.h"
#include <chrono>
#include <thread>

AutoAdvanceTracker::AutoAdvanceTracker(CommandSenderFunc sendCommand,
                                       LoadTrackFunc loadTrack)
    : sendCommand_(sendCommand), loadTrack_(loadTrack) {}

void AutoAdvanceTracker::start(std::atomic<bool> &stopFlag,
                               std::atomic<bool> &isPlaying,
                               std::vector<std::string> &tracks,
                               std::atomic<int> &currentIndex,
                               mpv_handle *mpv) {
  if (thread_ && thread_->joinable())
    return;
  running_ = true;
  mpvHandle = mpv;
  thread_ = std::make_unique<std::thread>(
      [this, &stopFlag, &isPlaying, &tracks, &currentIndex]() {
        run(stopFlag, isPlaying, tracks, currentIndex);
      });
}

void AutoAdvanceTracker::stop() {
  running_ = false;
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
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
  int consecutiveErrors = 0;
  while (!stopFlag && running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
    if (stopFlag || currentIndex < 0 || currentIndex >= (int)tracks.size())
      continue;
    if (!mpvHandle) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    try {
      int isPaused = 0;
      if (mpv_get_property(mpvHandle, "pause", MPV_FORMAT_FLAG, &isPaused) <
          0) {
        consecutiveErrors++;
        if (consecutiveErrors > 3) {
          std::this_thread::sleep_for(std::chrono::milliseconds(2000));
          consecutiveErrors = 0;
        }
        continue;
      }
      double currentTime = 0;
      double duration = 0;
      mpv_get_property(mpvHandle, "time-pos", MPV_FORMAT_DOUBLE, &currentTime);
      mpv_get_property(mpvHandle, "duration", MPV_FORMAT_DOUBLE, &duration);
      consecutiveErrors = 0;
      if (!isPaused && currentTime > 0) {
        wasPlaying = true;
        lastTrackTime = currentTime;
      }
      if (wasPlaying && !isPaused && currentTime < 0.5 && duration > 0 &&
          currentTime != lastTrackTime) {
        finishCount++;
        if (finishCount >= 2) {
          if (currentIndex + 1 < (int)tracks.size()) {
            loadTrack_(currentIndex + 1);
            finishCount = 0;
            wasPlaying = false;
            lastTrackTime = 0;
          } else {
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
    } catch (const std::exception &e) {
      consecutiveErrors++;
      if (consecutiveErrors > 5)
        break;
    }
  }
}
