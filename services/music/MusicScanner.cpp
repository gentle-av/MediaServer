#include "services/music/MusicScanner.h"
#include "database/MusicDatabase.h"
#include "models/MusicMetadata.h"
#include "services/music/MetadataExtractor.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

MusicScanner::MusicScanner(MusicDatabase &db, MetadataCache &cache,
                           const std::string &musicDir)
    : db_(db), cache_(cache), musicDir_(musicDir) {
  std::cout << "[MusicScanner] Created with musicDir: " << musicDir
            << std::endl;
}

MusicScanner::~MusicScanner() {
  std::cout << "[MusicScanner] Destructor" << std::endl;
  if (rescanThread_ && rescanThread_->joinable()) {
    rescanThread_->join();
  }
}

bool MusicScanner::isMusicFile(const std::string &path) {
  auto ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".mp3" || ext == ".flac" || ext == ".m4a" || ext == ".wav" ||
         ext == ".ogg";
}

std::vector<std::string> MusicScanner::scanMusicDirectory() {
  std::vector<std::string> musicFiles;
  if (!fs::exists(musicDir_)) {
    std::cerr << "[MusicScanner] Music directory does not exist: " << musicDir_
              << std::endl;
    return musicFiles;
  }
  try {
    for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
      if (entry.is_regular_file() && isMusicFile(entry.path().string())) {
        musicFiles.push_back(entry.path().string());
      }
    }
    std::cout << "[MusicScanner] Found " << musicFiles.size() << " music files"
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[MusicScanner] Error scanning directory: " << e.what()
              << std::endl;
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

bool MusicScanner::shouldProcessFile(const std::string &path,
                                     bool skipExistingInDb) {
  if (!skipExistingInDb) {
    return true;
  }
  MusicMetadata *cached = cache_.get(path);
  if (cached != nullptr) {
    return false;
  }
  if (db_.fileExists(path)) {
    MusicMetadata dbMetadata;
    if (db_.getMetadata(path, dbMetadata)) {
      cache_.put(path, dbMetadata);
      return false;
    }
  }
  return true;
}

void MusicScanner::scanNewFiles(bool skipExistingInDb) {
  if (status_.inProgress) {
    std::cout << "[MusicScanner] Scan already in progress" << std::endl;
    return;
  }
  std::thread([this, skipExistingInDb]() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto existingFiles = db_.getAllFiles();
    std::unordered_set<std::string> existingSet(existingFiles.begin(),
                                                existingFiles.end());
    auto musicFiles = scanMusicDirectory();
    status_.totalFiles = static_cast<int>(musicFiles.size());
    status_.processedFiles = 0;
    for (const auto &path : musicFiles) {
      if (shouldProcessFile(path, skipExistingInDb)) {
        processFile(path, true);
      }
      status_.processedFiles++;
    }
    std::cout << "[MusicScanner] Scan completed" << std::endl;
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
  if (status_.inProgress) {
    std::cout << "[MusicScanner] Rescan already in progress" << std::endl;
    if (onComplete)
      onComplete();
    return;
  }
  doRescan(onComplete);
}

void MusicScanner::doRescan(std::function<void()> onComplete) {
  if (rescanThread_ && rescanThread_->joinable()) {
    rescanThread_->join();
  }
  status_.reset();
  status_.inProgress = true;
  status_.startTime = std::chrono::steady_clock::now();
  status_.lastScanTime = status_.startTime;
  rescanThread_ = std::make_unique<std::thread>([this, onComplete]() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
      std::cout << "[MusicScanner] Starting force rescan" << std::endl;
      auto oldAlbums = db_.getAlbums();
      status_.oldAlbumsCount = static_cast<int>(oldAlbums.size());
      auto dbFiles = db_.getAllFiles();
      std::unordered_set<std::string> dbFilesSet(dbFiles.begin(),
                                                 dbFiles.end());
      auto musicFiles = scanMusicDirectory();
      status_.totalFiles = static_cast<int>(musicFiles.size());
      std::unordered_set<std::string> foundFiles;
      status_.addedFiles = 0;
      status_.errorCount = 0;
      status_.processedFiles = 0;
      for (const auto &path : musicFiles) {
        foundFiles.insert(path);
        if (dbFilesSet.find(path) == dbFilesSet.end()) {
          processFile(path, true);
        }
        status_.processedFiles++;
      }
      for (const auto &path : dbFiles) {
        if (foundFiles.find(path) == foundFiles.end()) {
          db_.removeFile(path);
          cache_.erase(path);
        }
      }
      auto newAlbums = db_.getAlbums();
      status_.newAlbumsCount = static_cast<int>(newAlbums.size());
      std::cout << "[MusicScanner] Force rescan completed" << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[MusicScanner] Force rescan error: " << e.what()
                << std::endl;
      status_.errorCount++;
    }
    status_.inProgress = false;
    if (onComplete) {
      onComplete();
    }
  });
}
