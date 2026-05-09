#pragma once

#include "database/MusicDatabase.h"
#include "services/music/MetadataCache.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct RescanStatus {
  std::atomic<bool> inProgress{false};
  std::atomic<int> totalFiles{0};
  std::atomic<int> processedFiles{0};
  std::atomic<int> addedFiles{0};
  std::atomic<int> errorCount{0};
  std::atomic<int> oldAlbumsCount{0};
  std::atomic<int> newAlbumsCount{0};
  std::chrono::steady_clock::time_point startTime;
  std::chrono::steady_clock::time_point lastScanTime;

  void reset() {
    inProgress = false;
    totalFiles = 0;
    processedFiles = 0;
    addedFiles = 0;
    errorCount = 0;
    oldAlbumsCount = 0;
    newAlbumsCount = 0;
  }
};

class MusicScanner {
public:
  MusicScanner(MusicDatabase &db, MetadataCache &cache,
               const std::string &musicDir);
  ~MusicScanner();

  void scanNewFiles(bool skipExistingInDb = true);
  void removeMissingFiles();
  void forceRescan(std::function<void()> onComplete);
  bool isInProgress() const { return status_.inProgress.load(); }
  const RescanStatus &getStatus() const { return status_; }

private:
  bool isMusicFile(const std::string &path);
  std::vector<std::string> scanMusicDirectory();
  void processFile(const std::string &path, bool addToDb);
  void doRescan(std::function<void()> onComplete);
  bool shouldProcessFile(const std::string &path, bool skipExistingInDb);

  MusicDatabase &db_;
  MetadataCache &cache_;
  std::string musicDir_;
  RescanStatus status_;
  std::mutex mutex_;
  std::unique_ptr<std::thread> rescanThread_;
  std::unordered_set<std::string> processedPaths_;
};
