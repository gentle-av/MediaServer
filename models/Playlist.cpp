#include "Playlist.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

Playlist::Playlist(const std::vector<MusicMetadata> &tracks) : tracks(tracks) {
  if (!tracks.empty())
    currentIndex = 0;
}

Playlist::Playlist(std::vector<MusicMetadata> &&tracks)
    : tracks(std::move(tracks)) {
  if (!tracks.empty())
    currentIndex = 0;
}

void Playlist::addTrack(const MusicMetadata &track) {
  tracks.push_back(track);
  if (currentIndex == -1)
    currentIndex = 0;
}

void Playlist::addTrack(const std::string &path) {
  MusicMetadata meta;
  meta.filePath = path;
  tracks.push_back(meta);
  if (currentIndex == -1)
    currentIndex = 0;
}

void Playlist::addTracks(const std::vector<MusicMetadata> &tracks) {
  if (tracks.empty())
    return;
  this->tracks.insert(tracks.end(), tracks.begin(), tracks.end());
  if (currentIndex == -1)
    currentIndex = 0;
}

void Playlist::removeTrack(int index) {
  if (index < 0 || index >= static_cast<int>(tracks.size()))
    return;
  tracks.erase(tracks.begin() + index);
  if (currentIndex >= static_cast<int>(tracks.size())) {
    currentIndex = tracks.empty() ? -1 : static_cast<int>(tracks.size()) - 1;
  }
}

void Playlist::clear() {
  tracks.clear();
  currentIndex = -1;
}

void Playlist::shuffle() {
  if (tracks.size() < 2)
    return;
  static std::random_device rd;
  static std::mt19937 g(rd());
  std::shuffle(tracks.begin(), tracks.end(), g);
  currentIndex = 0;
}

std::optional<MusicMetadata> Playlist::getTrack(int index) const {
  if (index < 0 || index >= static_cast<int>(tracks.size())) {
    return std::nullopt;
  }
  return tracks[index];
}

std::vector<MusicMetadata> Playlist::getAllTracks() const { return tracks; }

void Playlist::setCurrentIndex(int index) {
  if (index < -1 || index >= static_cast<int>(tracks.size()))
    return;
  currentIndex = index;
}

bool Playlist::save(const std::string &filePath) const {
  nlohmann::json j;
  j["current_index"] = currentIndex;
  for (const auto &t : tracks) {
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
  tracks.clear();
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
      tracks.push_back(t);
    }
  }
  if (tracks.empty()) {
    return false;
  }
  currentIndex = j.value("current_index", 0);
  if (currentIndex < 0 || currentIndex >= static_cast<int>(tracks.size())) {
    currentIndex = 0;
  }
  return true;
}

bool Playlist::removeByFilePath(const std::string &filePath) {
  for (auto it = tracks.begin(); it != tracks.end(); ++it) {
    if (it->filePath == filePath) {
      tracks.erase(it);
      if (currentIndex >= static_cast<int>(tracks.size())) {
        currentIndex =
            tracks.empty() ? -1 : static_cast<int>(tracks.size()) - 1;
      }
      return true;
    }
  }
  return false;
}

int Playlist::removeMissingTracks(
    std::function<bool(const std::string &)> fileExists) {
  int removed = 0;
  std::vector<int> indicesToRemove;
  for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
    if (!fileExists(tracks[i].filePath)) {
      indicesToRemove.push_back(i);
    }
  }
  for (auto it = indicesToRemove.rbegin(); it != indicesToRemove.rend(); ++it) {
    tracks.erase(tracks.begin() + *it);
    removed++;
  }
  if (removed > 0) {
    currentIndex = tracks.empty() ? -1 : 0;
  }
  return removed;
}

bool Playlist::hasTrack(const std::string &filePath) const {
  for (const auto &track : tracks) {
    if (track.filePath == filePath)
      return true;
  }
  return false;
}
