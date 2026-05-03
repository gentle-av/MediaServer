#include "services/MusicScanner.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicScanner &MusicScanner::getInstance() {
  static MusicScanner instance;
  return instance;
}

void MusicScanner::scanNewFiles() {
  std::thread([this]() {
    auto dbFiles = db_->getAllFiles();
    std::unordered_set<std::string> existingFiles(dbFiles.begin(),
                                                  dbFiles.end());
    std::vector<fs::path> musicFiles;
    if (fs::exists(musicDir_)) {
      for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
        if (entry.is_regular_file()) {
          auto ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" ||
              ext == ".wav") {
            musicFiles.push_back(entry.path());
          }
        }
      }
    }
    for (const auto &filePath : musicFiles) {
      std::string pathStr = filePath.string();
      if (existingFiles.find(pathStr) == existingFiles.end()) {
        MusicMetadata metadata;
        if (MetadataService::getInstance().extractMetadata(pathStr, metadata)) {
          if (db_->addFile(pathStr, metadata)) {
            std::vector<char> albumArt;
            if (MetadataService::getInstance().extractAlbumArt(pathStr,
                                                               albumArt)) {
              db_->saveAlbumArt(pathStr, albumArt);
            }
          }
        }
      }
    }
  }).detach();
}

void MusicScanner::removeMissingFiles() {
  auto allFiles = db_->getAllFiles();
  for (const auto &filePath : allFiles) {
    if (!fs::exists(filePath)) {
      db_->removeFile(filePath);
    }
  }
}

MusicScanner::RescanStatus MusicScanner::getStatus() {
  std::lock_guard<std::mutex> lock(statusMutex_);
  return status_;
}

void MusicScanner::forceRescan(std::function<void(int, int)> progressCallback) {
  {
    std::lock_guard<std::mutex> lock(statusMutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - status_.lastRescanTime)
                       .count();
    if (status_.inProgress || elapsed < MIN_RESCAN_INTERVAL_SEC) {
      return;
    }
    status_ = RescanStatus();
    status_.inProgress = true;
    status_.lastRescanTime = now;
  }
  std::thread([this, progressCallback]() {
    auto oldAlbums = db_->getAlbums();
    {
      std::lock_guard<std::mutex> lock(statusMutex_);
      status_.oldAlbumsCount = oldAlbums.size();
    }
    std::vector<fs::path> musicFiles;
    if (fs::exists(musicDir_)) {
      for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
        if (entry.is_regular_file()) {
          auto ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" ||
              ext == ".wav") {
            musicFiles.push_back(entry.path());
          }
        }
      }
    }
    {
      std::lock_guard<std::mutex> lock(statusMutex_);
      status_.totalFiles = musicFiles.size();
    }
    auto allFiles = db_->getAllFiles();
    for (const auto &filePath : allFiles) {
      db_->removeFile(filePath);
    }
    for (size_t i = 0; i < musicFiles.size(); i++) {
      if (stopRescan_)
        break;
      std::string pathStr = musicFiles[i].string();
      MusicMetadata metadata;
      if (MetadataService::getInstance().extractMetadata(pathStr, metadata)) {
        if (db_->addFile(pathStr, metadata)) {
          std::lock_guard<std::mutex> lock(statusMutex_);
          status_.addedFiles++;
          std::vector<char> albumArt;
          if (MetadataService::getInstance().extractAlbumArt(pathStr,
                                                             albumArt)) {
            db_->saveAlbumArt(pathStr, albumArt);
          }
        } else {
          std::lock_guard<std::mutex> lock(statusMutex_);
          status_.errorCount++;
        }
      } else {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status_.errorCount++;
      }
      {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status_.processedFiles = i + 1;
      }
      if (progressCallback) {
        progressCallback(i + 1, musicFiles.size());
      }
    }
    auto newAlbums = db_->getAlbums();
    {
      std::lock_guard<std::mutex> lock(statusMutex_);
      status_.newAlbumsCount = newAlbums.size();
      status_.inProgress = false;
    }
  }).detach();
}
