#pragma once

#include "VideoThumbnailer.h"
#include "database/ImageDatabase.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class ThumbnailBatchScanner {
public:
  struct Progress {
    int totalFiles = 0;
    int processedFiles = 0;
    int successfulThumbnails = 0;
    int failedFiles = 0;
    bool isRunning = false;
    std::string currentFile;
  };

  explicit ThumbnailBatchScanner(ImageDatabase &database);
  ~ThumbnailBatchScanner();

  void
  startScan(const std::string &directory, bool forceRegenerate = false,
            std::function<void(const Progress &)> progressCallback = nullptr);

  void stopScan();
  bool isRunning() const { return running; }
  Progress getProgress() const;

private:
  void scanThreadFunc(const std::string &directory, bool forceRegenerate,
                      std::function<void(const Progress &)> progressCallback);

  bool isVideoFile(const std::filesystem::path &path) const;
  bool shouldProcessFile(const std::filesystem::path &path,
                         bool forceRegenerate) const;
  bool processVideo(const std::filesystem::path &path);

  ImageDatabase &database;
  VideoThumbnailer thumbnailer;

  std::atomic<bool> running{false};
  std::atomic<bool> stopRequested{false};
  Progress progress;
  mutable std::mutex progressMutex;
  std::unique_ptr<std::thread> scanThread;
};
