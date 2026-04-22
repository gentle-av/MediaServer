#pragma once

#include <cstdint>
#include <string>
#include <vector>

class ThumbnailExtractor {
public:
  static std::string generateThumbnailBase64(const std::string &videoPath,
                                             int width = 320, int quality = 85);
  static void initCache(const std::string &dbPath = "");
  static void clearCache();
  static void shutdownCache();
  static std::string base64Encode(const std::vector<uint8_t> &data);
  static bool generateRawThumbnail(const std::string &videoPath, int width,
                                   int quality,
                                   std::vector<uint8_t> &imageData);
};
