#pragma once

#include "MusicMetadata.h"
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
  int getCurrentIndex() const { return currentIndex_; }
  void setCurrentIndex(int index);
  size_t size() const { return tracks_.size(); }
  bool empty() const { return tracks_.empty(); }

  bool save(const std::string &filePath) const;
  bool load(const std::string &filePath);

private:
  std::vector<MusicMetadata> tracks_;
  int currentIndex_ = -1;
};
