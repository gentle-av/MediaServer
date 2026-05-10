#pragma once

#include <string>

class PlaybackService {
public:
  static PlaybackService &getInstance();
  void openVideo(const std::string &path, std::string &activeSocket,
                 bool &success);
  void closeVideo(std::string &activeSocket);
  void forceStop(std::string &activeSocket);
  bool sendCommand(const std::string &activeSocket, const std::string &command,
                   std::string &response);
  bool seek(const std::string &activeSocket, double seekTime,
            std::string &response);
  bool getProperty(const std::string &activeSocket, const std::string &property,
                   std::string &value);
  bool checkProcessAlive(const std::string &activeSocket);

private:
  PlaybackService() = default;
  int socketCounter = 0;
};
