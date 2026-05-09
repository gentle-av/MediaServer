#pragma once

#include <json/json.h>
#include <string>

class VideoControlHandler {
public:
  static VideoControlHandler &getInstance();
  Json::Value handleOpen(const std::string &path, std::string &activeSocket);
  Json::Value handleClose(std::string &activeSocket);
  Json::Value handleForceStop(std::string &activeSocket);
  Json::Value handleControl(const std::string &command,
                            std::string &activeSocket);
  Json::Value handleSeek(double seekTime, std::string &activeSocket);
  Json::Value handleGetProperty(const std::string &propertyName,
                                const std::string &activeSocket);

private:
  VideoControlHandler() = default;
};
