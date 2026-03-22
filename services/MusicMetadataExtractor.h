#pragma once

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

struct MusicMetadata {
  std::string title;
  std::string artist;
  std::string album;
  std::string genre;
  int year = 0;
  int trackNumber = 0;
  std::string filePath;
  bool isValid = false;
};

class MusicMetadataExtractor {
public:
  static MusicMetadata extractMetadata(const std::string &filePath);
  static std::vector<std::string> getArtists(const std::string &rootPath);
  static std::vector<std::pair<std::string, std::string>>
  getAlbumsByArtist(const std::string &rootPath, const std::string &artist);
  static std::vector<std::tuple<std::string, std::string, std::string>>
  getAllAlbums(const std::string &rootPath);

private:
  static bool isAudioFile(const std::string &path);
  static void scanDirectory(const fs::path &path,
                            std::vector<MusicMetadata> &results,
                            int maxDepth = 5);
};
