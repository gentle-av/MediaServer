#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <atomic>
#include <chrono>
#include <string>

class MusicScanner {
public:
  struct ScanStatus {
    std::atomic<bool> inProgress{false};
    std::atomic<int> totalFiles{0};
    std::atomic<int> processedFiles{0};
    std::atomic<int> addedFiles{0};
    std::atomic<int> errorCount{0};
    std::atomic<int> oldAlbumsCount{0};
    std::atomic<int> newAlbumsCount{0};
    std::chrono::steady_clock::time_point lastScanTime;
  };

  MusicScanner(MusicDatabase &db, MetadataCache &cache,
               const std::string &musicDir);

  void scanNewFiles();
  void removeMissingFiles();
  void forceRescan(std::function<void()> onComplete = nullptr);
  const ScanStatus &getStatus() const { return status_; }
  bool isInProgress() const { return status_.inProgress; }

private:
  std::vector<std::string> scanMusicDirectory();
  bool isMusicFile(const std::string &path);
  void processFile(const std::string &path, bool addToDb = true);

  MusicDatabase &db_;
  MetadataCache &cache_;
  std::string musicDir_;
  ScanStatus status_;
};
