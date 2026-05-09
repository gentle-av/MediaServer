#pragma once

#include <json/json.h>
#include <string>

class PlaybackStatus {
public:
  static PlaybackStatus &getInstance();
  Json::Value getStatus(const std::string &activeSocket);
  bool seek(const std::string &activeSocket, double seekTime,
            double &actualTime, double &duration);

private:
  PlaybackStatus() = default;
  double parsePropertyValue(const std::string &response,
                            const std::string &propertyName);
};
