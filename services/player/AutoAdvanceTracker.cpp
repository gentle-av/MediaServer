#include "AutoAdvanceTracker.h"

void AutoAdvanceTracker::start(std::atomic<bool> &, std::atomic<bool> &,
                               std::vector<std::string> &, std::atomic<int> &,
                               mpv_handle *) {}

void AutoAdvanceTracker::stop() {}

bool AutoAdvanceTracker::isRunning() const { return false; }
