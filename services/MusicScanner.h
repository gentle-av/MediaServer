#pragma once

#include "database/MusicDatabase.h"
#include "services/MetadataService.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

class MusicScanner {
public:
  static MusicScanner &getInstance();

  void setDatabase(MusicDatabase *db) { db_ = db; }
  void setMusicDir(const std::string &dir) { musicDir_ = dir; }

  void scanNewFiles();
  void removeMissingFiles();
  void forceRescan(std::function<void(int, int)> progressCallback = nullptr);

  struct RescanStatus {
    bool inProgress = false;
    int totalFiles = 0;
    int processedFiles = 0;
    int addedFiles = 0;
    int errorCount = 0;
    int oldAlbumsCount = 0;
    int newAlbumsCount = 0;
    std::chrono::steady_clock::time_point lastRescanTime;
  };

  RescanStatus getStatus();
  bool isInProgress() const { return status_.inProgress; }

private:
  MusicScanner() = default;

  MusicDatabase *db_ = nullptr;
  std::string musicDir_;
  RescanStatus status_;
  std::mutex statusMutex_;
  std::thread rescanThread_;
  std::atomic<bool> stopRescan_{false};

  static constexpr int MIN_RESCAN_INTERVAL_SEC = 5;
};
