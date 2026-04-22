#include "ThumbnailExtractor.h"
#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>
#include <stdexcept>
#include <vector>

std::string
ThumbnailExtractor::generateThumbnailBase64(const std::string &videoPath,
                                            int width, int quality) {
  try {
    ffmpegthumbnailer::VideoThumbnailer thumbnailer;
    thumbnailer.setThumbnailSize(width);
    thumbnailer.setSeekPercentage(50);
    thumbnailer.setImageQuality(quality);
    thumbnailer.setMaintainAspectRatio(true);
    std::vector<uint8_t> imageData;
    thumbnailer.generateThumbnail(videoPath, Jpeg, imageData);
    if (imageData.empty()) {
      return "";
    }
    return base64Encode(imageData);
  } catch (const std::exception &) {
    return "";
  }
}

std::string ThumbnailExtractor::base64Encode(const std::vector<uint8_t> &data) {
  static const char *base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string base64;
  base64.reserve(((data.size() + 2) / 3) * 4);
  for (size_t i = 0; i < data.size(); i += 3) {
    uint32_t block = 0;
    int padding = 0;
    block |= (uint32_t)data[i] << 16;
    if (i + 1 < data.size()) {
      block |= (uint32_t)data[i + 1] << 8;
    } else {
      padding++;
    }
    if (i + 2 < data.size()) {
      block |= (uint32_t)data[i + 2];
    } else {
      padding++;
    }
    base64.push_back(base64_chars[(block >> 18) & 0x3F]);
    base64.push_back(base64_chars[(block >> 12) & 0x3F]);
    base64.push_back(padding >= 2 ? '=' : base64_chars[(block >> 6) & 0x3F]);
    base64.push_back(padding >= 1 ? '=' : base64_chars[block & 0x3F]);
  }
  return base64;
}
