#include "services/player/MetadataParser.h"
#include <filesystem>

MusicMetadata MetadataParser::parse(const std::string &filePath) {
  MusicMetadata metadata;
  metadata.filePath = filePath;
  std::filesystem::path path(filePath);
  metadata.title = path.stem().string();
  return metadata;
}

std::string MetadataParser::extractTitle(const std::string &filePath) {
  return std::filesystem::path(filePath).stem().string();
}

std::string MetadataParser::extractArtist(const std::string &filePath) {
  return "";
}
