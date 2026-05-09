#pragma once

#include <json/json.h>
#include <string>
#include <vector>

class ThumbnailRequestHandler {
public:
  static ThumbnailRequestHandler &getInstance();
  Json::Value handleSingleRequest(const std::string &videoPath, int width,
                                  int quality);
  Json::Value handleBatchRequest(const std::vector<std::string> &paths,
                                 int width, int quality);

private:
  ThumbnailRequestHandler() = default;
  Json::Value buildErrorResponse(const std::string &error, bool useIcon = false,
                                 const std::string &ext = "");
  Json::Value buildSuccessResponse(const std::string &base64Thumbnail,
                                   int width, int quality,
                                   const std::string &path);
};
