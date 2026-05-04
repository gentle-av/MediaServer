#pragma once

#include <string>

struct MusicMetadata {
  std::string title;
  std::string artist;
  std::string album;
  int duration = 0;
  int track = 0;
  int year = 0;
  std::string genre;
  std::string filePath;

  MusicMetadata() = default;
  MusicMetadata(const MusicMetadata &) = default;
  MusicMetadata(MusicMetadata &&) = default;
  MusicMetadata &operator=(const MusicMetadata &) = default;
  MusicMetadata &operator=(MusicMetadata &&) = default;
  ~MusicMetadata() = default;
};
