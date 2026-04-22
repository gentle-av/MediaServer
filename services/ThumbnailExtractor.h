// services/ThumbnailExtractor.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class ThumbnailExtractor {
public:
  static std::string generateThumbnailBase64(const std::string &videoPath,
                                             int width = 320, int quality = 85);

private:
  static std::string base64Encode(const std::vector<uint8_t> &data);
};
