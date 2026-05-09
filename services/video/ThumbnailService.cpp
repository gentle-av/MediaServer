#include "ThumbnailService.h"
#include "../ThumbnailCache.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>
#include <vector>

namespace fs = std::filesystem;

ThumbnailService &ThumbnailService::getInstance() {
  static ThumbnailService instance;
  return instance;
}

bool ThumbnailService::isVideoValid(const std::string &videoPath) {
  std::string cmd =
      "ffprobe -v error -select_streams v:0 -show_streams -show_format \"" +
      videoPath + "\" 2>/dev/null";
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return false;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  return !result.empty();
}

void ThumbnailService::initCache(const std::string &dbPath) {
  if (!isCacheInitialized) {
    std::string path = dbPath;
    if (path.empty()) {
      const char *home = getenv("HOME");
      path = home ? std::string(home) + "/.local/share/media-explorer/video.db"
                  : "./video.db";
      fs::create_directories(fs::path(path).parent_path());
    }
    ThumbnailCache::getInstance().init(path);
    isCacheInitialized = true;
  }
}

bool ThumbnailService::generateRawThumbnail(const std::string &videoPath,
                                            int width, int quality,
                                            std::vector<uint8_t> &imageData) {
  if (!isVideoValid(videoPath)) {
    return false;
  }
  try {
    ffmpegthumbnailer::VideoThumbnailer thumbnailer;
    thumbnailer.setThumbnailSize(width);
    thumbnailer.setSeekPercentage(50);
    thumbnailer.setImageQuality(quality);
    thumbnailer.setMaintainAspectRatio(true);
    thumbnailer.generateThumbnail(videoPath, Jpeg, imageData);
    return !imageData.empty();
  } catch (const std::exception &) {
    return false;
  }
}

std::string
ThumbnailService::generateThumbnailBase64(const std::string &videoPath,
                                          int width, int quality) {
  initCache();
  return ThumbnailCache::getInstance().getThumbnail(videoPath, width, quality);
}

void ThumbnailService::clearCache() {
  if (isCacheInitialized) {
    ThumbnailCache::getInstance().clearCache();
  }
}

void ThumbnailService::shutdownCache() {
  if (isCacheInitialized) {
    ThumbnailCache::getInstance().shutdown();
    isCacheInitialized = false;
  }
}

std::string ThumbnailService::base64Encode(const std::vector<uint8_t> &data) {
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
