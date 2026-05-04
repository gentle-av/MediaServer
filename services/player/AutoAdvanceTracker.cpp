#include "services/player/AutoAdvanceTracker.h"
#include <chrono>
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
  bool waitingForNext = false;
  int waitCounter = 0;
  while (!stopFlag && running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (stopFlag || currentIndex < 0 || currentIndex >= (int)tracks.size())
      continue;
    std::string pauseResp =
        sendCommand_(R"({"command": ["get_property", "pause"]})");
    std::string timeResp =
        sendCommand_(R"({"command": ["get_property", "time-pos"]})");
    std::string durationResp =
        sendCommand_(R"({"command": ["get_property", "duration"]})");
    std::string eofResp =
        sendCommand_(R"({"command": ["get_property", "eof-reached"]})");
    bool isPaused = pauseResp.find("\"data\":true") != std::string::npos;
    double duration = 0;
    bool eofReached = eofResp.find("\"data\":true") != std::string::npos;
    size_t pos = durationResp.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = durationResp.find(":", pos);
      if (start != std::string::npos) {
        try {
          duration = std::stod(durationResp.substr(start + 1));
        } catch (...) {
        }
      }
    }
    if (eofReached && !waitingForNext && duration > 0) {
      waitingForNext = true;
      waitCounter = 0;
    }
    if (waitingForNext) {
      waitCounter++;
      if (duration > 0 || waitCounter > 8) {
        if (currentIndex + 1 < (int)tracks.size())
          loadTrack_(currentIndex + 1);
        else {
          isPlaying = false;
          currentIndex = -1;
        }
        waitingForNext = false;
        waitCounter = 0;
      }
    }
  }
}
