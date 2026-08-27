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
    : db(db), cache(cache), musicDir(musicDir) {
  std::cout << "[MusicScanner] Created with musicDir: " << musicDir
            << std::endl;
}

MusicScanner::~MusicScanner() {
  std::cout << "[MusicScanner] Destructor" << std::endl;
  if (rescanThread && rescanThread->joinable()) {
    rescanThread->join();
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
  if (!fs::exists(musicDir)) {
    std::cerr << "[MusicScanner] Music directory does not exist: " << musicDir
              << std::endl;
    return musicFiles;
  }
  try {
    for (const auto &entry : fs::recursive_directory_iterator(musicDir)) {
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
    if (addToDb && db.addFile(path, metadata)) {
      status.addedFiles++;
      std::vector<char> albumArt;
      if (MetadataExtractor::extractAlbumArt(path, albumArt)) {
        db.saveAlbumArt(path, albumArt);
      }
    }
    cache.put(path, metadata);
  } else {
    status.errorCount++;
  }
}

bool MusicScanner::shouldProcessFile(const std::string &path,
                                     bool skipExistingInDb) {
  if (!skipExistingInDb) {
    return true;
  }
  MusicMetadata *cached = cache.get(path);
  if (cached != nullptr) {
    return false;
  }
  if (db.fileExists(path)) {
    MusicMetadata dbMetadata;
    if (db.getMetadata(path, dbMetadata)) {
      cache.put(path, dbMetadata);
      return false;
    }
  }
  return true;
}

void MusicScanner::scanNewFiles(bool skipExistingInDb) {
  if (status.inProgress) {
    std::cout << "[MusicScanner] Scan already in progress" << std::endl;
    return;
  }
  std::thread([this, skipExistingInDb]() {
    std::lock_guard<std::mutex> lock(mutex);
    auto existingFiles = db.getAllFiles();
    std::unordered_set<std::string> existingSet(existingFiles.begin(),
                                                existingFiles.end());
    auto musicFiles = scanMusicDirectory();
    status.totalFiles = static_cast<int>(musicFiles.size());
    status.processedFiles = 0;
    for (const auto &path : musicFiles) {
      if (shouldProcessFile(path, skipExistingInDb)) {
        processFile(path, true);
      }
      status.processedFiles++;
    }
    std::cout << "[MusicScanner] Scan completed" << std::endl;
  }).detach();
}

void MusicScanner::removeMissingFiles() {
  auto allFiles = db.getAllFiles();
  for (const auto &path : allFiles) {
    if (!fs::exists(path)) {
      db.removeFile(path);
      cache.erase(path);
    }
  }
}

void MusicScanner::forceRescan(std::function<void()> onComplete) {
  if (status.inProgress) {
    std::cout << "[MusicScanner] Rescan already in progress" << std::endl;
    if (onComplete)
      onComplete();
    return;
  }
  doRescan(onComplete);
}

void MusicScanner::doRescan(std::function<void()> onComplete) {
  if (rescanThread && rescanThread->joinable()) {
    rescanThread->join();
  }
  status.reset();
  status.inProgress = true;
  status.startTime = std::chrono::steady_clock::now();
  status.lastScanTime = status.startTime;
  rescanThread = std::make_unique<std::thread>([this, onComplete]() {
    std::lock_guard<std::mutex> lock(mutex);
    try {
      std::cout << "[MusicScanner] Starting force rescan" << std::endl;
      auto oldAlbums = db.getAlbums();
      status.oldAlbumsCount = static_cast<int>(oldAlbums.size());
      auto dbFiles = db.getAllFiles();
      std::unordered_set<std::string> dbFilesSet(dbFiles.begin(),
                                                 dbFiles.end());
      auto musicFiles = scanMusicDirectory();
      status.totalFiles = static_cast<int>(musicFiles.size());
      std::unordered_set<std::string> foundFiles;
      status.addedFiles = 0;
      status.errorCount = 0;
      status.processedFiles = 0;
      for (const auto &path : musicFiles) {
        foundFiles.insert(path);
        if (dbFilesSet.find(path) == dbFilesSet.end()) {
          processFile(path, true);
        }
        status.processedFiles++;
      }
      for (const auto &path : dbFiles) {
        if (foundFiles.find(path) == foundFiles.end()) {
          db.removeFile(path);
          cache.erase(path);
        }
      }
      auto newAlbums = db.getAlbums();
      status.newAlbumsCount = static_cast<int>(newAlbums.size());
      std::cout << "[MusicScanner] Force rescan completed" << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[MusicScanner] Force rescan error: " << e.what()
                << std::endl;
      status.errorCount++;
    }
    status.inProgress = false;
    if (onComplete) {
      onComplete();
    }
  });
}
