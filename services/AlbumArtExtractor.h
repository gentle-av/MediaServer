#pragma once

#include <memory>
#include <string>
#include <taglib/flacfile.h>
#include <vector>

class AlbumArtExtractor {
public:
  struct AlbumArt {
    std::vector<char> data;
    std::string mimeType;
    std::string description;
  };
  static std::unique_ptr<AlbumArt> extractAlbumArt(const std::string &filePath);
  static bool isSupportedFormat(const std::string &filePath);
  static std::string getMimeTypeFromData(const std::vector<char> &data);

private:
  static std::unique_ptr<AlbumArt> extractFromFlac(const std::string &filePath);
  static std::string detectImageType(const std::vector<char> &data);
};
