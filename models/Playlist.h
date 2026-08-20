#pragma once

#include "MusicMetadata.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

class Playlist {
public:
  explicit Playlist(const std::vector<MusicMetadata> &tracks);
  explicit Playlist(std::vector<MusicMetadata> &&tracks);

  void addTrack(const MusicMetadata &track);
  void addTrack(const std::string &path);
  void addTracks(const std::vector<MusicMetadata> &tracks);
  void removeTrack(int index);
  void clear();
  void shuffle();

  std::optional<MusicMetadata> getTrack(int index) const;
  std::vector<MusicMetadata> getAllTracks() const;
  int getCurrentIndex() const { return currentIndex; }
  void setCurrentIndex(int index);
  size_t size() const { return tracks.size(); }
  bool empty() const { return tracks.empty(); }

  bool save(const std::string &filePath) const;
  bool load(const std::string &filePath);

  bool removeByFilePath(const std::string &filePath);
  int removeMissingTracks(std::function<bool(const std::string &)> fileExists);
  bool hasTrack(const std::string &filePath) const;

private:
  std::vector<MusicMetadata> tracks;
  int currentIndex = -1;
};
