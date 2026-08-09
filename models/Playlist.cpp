#include "Playlist.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

Playlist::Playlist(const std::vector<MusicMetadata> &tracks) : tracks_(tracks) {
  if (!tracks_.empty())
    currentIndex_ = 0;
}

Playlist::Playlist(std::vector<MusicMetadata> &&tracks)
    : tracks_(std::move(tracks)) {
  if (!tracks_.empty())
    currentIndex_ = 0;
}

void Playlist::addTrack(const MusicMetadata &track) {
  tracks_.push_back(track);
  if (currentIndex_ == -1)
    currentIndex_ = 0;
}

void Playlist::addTrack(const std::string &path) {
  MusicMetadata meta;
  meta.filePath = path;
  tracks_.push_back(meta);
  if (currentIndex_ == -1)
    currentIndex_ = 0;
}

void Playlist::addTracks(const std::vector<MusicMetadata> &tracks) {
  if (tracks.empty())
    return;
  tracks_.insert(tracks_.end(), tracks.begin(), tracks.end());
  if (currentIndex_ == -1)
    currentIndex_ = 0;
}

void Playlist::removeTrack(int index) {
  if (index < 0 || index >= static_cast<int>(tracks_.size()))
    return;
  tracks_.erase(tracks_.begin() + index);
  if (currentIndex_ >= static_cast<int>(tracks_.size())) {
    currentIndex_ = tracks_.empty() ? -1 : static_cast<int>(tracks_.size()) - 1;
  }
}

void Playlist::clear() {
  tracks_.clear();
  currentIndex_ = -1;
}

void Playlist::shuffle() {
  if (tracks_.size() < 2)
    return;
  static std::random_device rd;
  static std::mt19937 g(rd());
  std::shuffle(tracks_.begin(), tracks_.end(), g);
  currentIndex_ = 0;
}

std::optional<MusicMetadata> Playlist::getTrack(int index) const {
  if (index < 0 || index >= static_cast<int>(tracks_.size())) {
    return std::nullopt;
  }
  return tracks_[index];
}

std::vector<MusicMetadata> Playlist::getAllTracks() const { return tracks_; }

void Playlist::setCurrentIndex(int index) {
  if (index < -1 || index >= static_cast<int>(tracks_.size()))
    return;
  currentIndex_ = index;
}

bool Playlist::save(const std::string &filePath) const {
  nlohmann::json j;
  j["current_index"] = currentIndex_;
  for (const auto &t : tracks_) {
    nlohmann::json tj;
    tj["file_path"] = t.filePath;
    tj["title"] = t.title;
    tj["artist"] = t.artist;
    tj["album"] = t.album;
    tj["duration"] = t.duration;
    tj["track"] = t.track;
    tj["year"] = t.year;
    tj["genre"] = t.genre;
    j["tracks"].push_back(tj);
  }
  std::ofstream file(filePath);
  if (!file.is_open())
    return false;
  file << j.dump(2);
  return true;
}

bool Playlist::load(const std::string &filePath) {
  std::ifstream file(filePath);
  if (!file.is_open())
    return false;
  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }
  tracks_.clear();
  if (j.contains("tracks")) {
    for (const auto &tj : j["tracks"]) {
      MusicMetadata t;
      t.filePath = tj.value("file_path", "");
      t.title = tj.value("title", "");
      t.artist = tj.value("artist", "");
      t.album = tj.value("album", "");
      t.duration = tj.value("duration", 0);
      t.track = tj.value("track", 0);
      t.year = tj.value("year", 0);
      t.genre = tj.value("genre", "");
      tracks_.push_back(t);
    }
  }
  if (tracks_.empty()) {
    return false;
  }
  currentIndex_ = j.value("current_index", 0);
  if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(tracks_.size())) {
    currentIndex_ = 0;
  }
  return true;
}
