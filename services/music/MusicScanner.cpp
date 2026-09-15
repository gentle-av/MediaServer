#include "services/music/MusicScanner.h"
#include "database/MusicDatabase.h"
#include "models/MusicMetadata.h"
#include "services/music/MetadataExtractor.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicScanner::MusicScanner(MusicDatabase &db, MetadataCache &cache,
                           const std::string &musicDir)
    : db(db), cache(cache), musicDir(musicDir) {}

MusicScanner::~MusicScanner() {
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
    return musicFiles;
  }
  try {
    for (const auto &entry : fs::recursive_directory_iterator(musicDir)) {
      if (entry.is_regular_file() && isMusicFile(entry.path().string())) {
        musicFiles.push_back(entry.path().string());
      }
    }
  } catch (...) {
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
    return;
  }
  std::thread([this, skipExistingInDb]() {
    std::lock_guard<std::mutex> lock(mutex);
    auto existingFiles = db.getAllFilePaths();
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
  }).detach();
}

void MusicScanner::removeMissingFiles() {
  auto allFiles = db.getAllFilePaths();
  for (const auto &path : allFiles) {
    if (!fs::exists(path)) {
      db.removeFile(path);
      cache.erase(path);
    }
  }
}

void MusicScanner::forceRescan(std::function<void()> onComplete) {
  if (status.inProgress) {
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
      auto oldAlbums = db.getAlbumsRaw();
      status.oldAlbumsCount = static_cast<int>(oldAlbums.size());
      auto dbFiles = db.getAllFilePaths();
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
        processFile(path, true);
        status.processedFiles++;
      }
      for (const auto &path : dbFiles) {
        if (foundFiles.find(path) == foundFiles.end()) {
          db.removeFile(path);
          cache.erase(path);
        }
      }
      auto newAlbums = db.getAlbumsRaw();
      status.newAlbumsCount = static_cast<int>(newAlbums.size());
    } catch (...) {
      status.errorCount++;
    }
    status.inProgress = false;
    if (onComplete) {
      onComplete();
    }
  });
}
