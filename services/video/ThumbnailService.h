#pragma once

#include <cstdint>
#include <json/json.h>
#include <string>
#include <vector>

class ThumbnailService {
public:
  static ThumbnailService &getInstance();
  bool isVideoValid(const std::string &videoPath);
  std::string generateThumbnailBase64(const std::string &videoPath,
                                      int width = 320, int quality = 85);
  void initCache(const std::string &dbPath = "");
  void clearCache();
  void shutdownCache();
  bool generateRawThumbnail(const std::string &videoPath, int width,
                            int quality, std::vector<uint8_t> &imageData);
  std::string base64Encode(const std::vector<uint8_t> &data);
  Json::Value generateThumbnailResponse(const std::string &videoPath, int width,
                                        int quality);

private:
  ThumbnailService() = default;
  bool isCacheInitialized = false;
};
