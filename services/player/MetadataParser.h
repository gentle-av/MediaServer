#pragma once

#include "models/MusicMetadata.h"
#include <string>

class MetadataParser {
public:
  MusicMetadata parse(const std::string &filePath);
  std::string extractTitle(const std::string &filePath);
  std::string extractArtist(const std::string &filePath);
};
