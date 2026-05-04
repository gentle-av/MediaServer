#pragma once

#include "models/MusicMetadata.h"
#include <string>
#include <taglib/tstring.h>
#include <vector>

class MetadataExtractor {
public:
  static bool extractMetadata(const std::string &filePath,
                              MusicMetadata &metadata);
  static bool extractAlbumArt(const std::string &filePath,
                              std::vector<char> &albumArt);
  static bool updateFileTags(const std::string &filePath,
                             const MusicMetadata &metadata);

private:
  static std::string fixTagLibString(const TagLib::String &str);
  static bool extractWithTagLib(const std::string &filePath,
                                MusicMetadata &metadata);
  static bool extractFilenameFallback(const std::string &filePath,
                                      MusicMetadata &metadata);
  static bool extractWithTagEditor(const std::string &filePath,
                                   MusicMetadata &metadata);
  static bool extractFlacAlbumArt(const std::string &filePath,
                                  std::vector<char> &albumArt);
};
