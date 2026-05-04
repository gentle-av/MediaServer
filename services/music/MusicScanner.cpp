#include "services/music/MusicScanner.h"
#include "models/MusicMetadata.h"
#include "services/music/MetadataExtractor.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicScanner::MusicScanner(MusicDatabase &db, MetadataCache &cache,
                           const std::string &musicDir)
    : db_(db), cache_(cache), musicDir_(musicDir) {}

bool MusicScanner::isMusicFile(const std::string &path) {
  auto ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".mp3" || ext == ".flac" || ext == ".m4a" || ext == ".wav";
}

std::vector<std::string> MusicScanner::scanMusicDirectory() {
  std::vector<std::string> musicFiles;
  if (!fs::exists(musicDir_))
    return musicFiles;
  for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
    if (entry.is_regular_file() && isMusicFile(entry.path().string())) {
      musicFiles.push_back(entry.path().string());
    }
  }
  return musicFiles;
}

void MusicScanner::processFile(const std::string &path, bool addToDb) {
  MusicMetadata metadata;
  if (MetadataExtractor::extractMetadata(path, metadata)) {
    if (addToDb && db_.addFile(path, metadata)) {
      status_.addedFiles++;
      std::vector<char> albumArt;
      if (MetadataExtractor::extractAlbumArt(path, albumArt)) {
        db_.saveAlbumArt(path, albumArt);
      }
    }
    cache_.put(path, metadata);
  } else {
    status_.errorCount++;
  }
}

void MusicScanner::scanNewFiles() {
  std::thread([this]() {
    auto existingFiles = db_.getAllFiles();
    std::unordered_set<std::string> existingSet(existingFiles.begin(),
                                                existingFiles.end());
    auto musicFiles = scanMusicDirectory();
    status_.totalFiles = musicFiles.size();
    for (const auto &path : musicFiles) {
      if (existingSet.find(path) == existingSet.end()) {
        processFile(path, true);
      }
      status_.processedFiles++;
    }
  }).detach();
}

void MusicScanner::removeMissingFiles() {
  auto allFiles = db_.getAllFiles();
  for (const auto &path : allFiles) {
    if (!fs::exists(path)) {
      db_.removeFile(path);
      cache_.erase(path);
    }
  }
}

void MusicScanner::forceRescan(std::function<void()> onComplete) {
  if (status_.inProgress)
    return;
  status_.inProgress = true;
  status_.lastScanTime = std::chrono::steady_clock::now();
  std::thread([this, onComplete]() {
    try {
      auto oldAlbums = db_.getAlbums();
      status_.oldAlbumsCount = oldAlbums.size();
      auto musicFiles = scanMusicDirectory();
      status_.totalFiles = musicFiles.size();
      auto allFiles = db_.getAllFiles();
      for (const auto &path : allFiles) {
        db_.removeFile(path);
        cache_.erase(path);
      }
      status_.addedFiles = 0;
      status_.errorCount = 0;
      status_.processedFiles = 0;
      for (const auto &path : musicFiles) {
        processFile(path, true);
        status_.processedFiles++;
      }
      auto newAlbums = db_.getAlbums();
      status_.newAlbumsCount = newAlbums.size();
    } catch (const std::exception &) {
    }
    status_.inProgress = false;
    if (onComplete)
      onComplete();
  }).detach();
}
