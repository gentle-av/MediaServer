#include "services/player/TrackLoader.h"
#include <atomic>

TrackLoader::TrackLoader(SendCommandFunc sendCommand)
    : sendCommand_(sendCommand) {}

void TrackLoader::loadTrack(const std::string &path,
                            std::atomic<int> &currentIndex,
                            std::atomic<bool> &isPlaying) {
  isPlaying = true;
  std::string cmd = "{\"command\": [\"loadfile\", \"" + escapePath(path) +
                    "\", \"replace\"]}";
  sendCommand_(cmd);
}

std::string TrackLoader::escapePath(const std::string &path) {
  std::string escaped = path;
  size_t pos = 0;
  while ((pos = escaped.find("\"", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return escaped;
}
