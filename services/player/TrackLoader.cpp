#include "services/player/TrackLoader.h"
#include <atomic>
#include <iostream>

TrackLoader::TrackLoader(SendCommandFunc sendCommand)
    : sendCommand_(sendCommand) {}

void TrackLoader::loadTrack(const std::string &path,
                            std::atomic<int> &currentIndex,
                            std::atomic<bool> &isPlaying) {
  std::cout << "[DEBUG] TrackLoader::loadTrack: Loading file: " << path
            << std::endl;
  isPlaying = true;
  std::string escaped = escapePath(path);
  std::string cmd =
      "{\"command\": [\"loadfile\", \"" + escaped + "\", \"replace\"]}";
  std::cout << "[DEBUG] TrackLoader::loadTrack: Sending command: " << cmd
            << std::endl;
  std::string response = sendCommand_(cmd);
  std::cout << "[DEBUG] TrackLoader::loadTrack: Response: " << response
            << std::endl;
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
