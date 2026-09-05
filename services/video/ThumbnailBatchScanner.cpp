#include "ThumbnailBatchScanner.h"
#include "VideoIntegrityChecker.h"
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

ThumbnailBatchScanner::ThumbnailBatchScanner(ImageDatabase &database)
    : database(database) {}

ThumbnailBatchScanner::~ThumbnailBatchScanner() { stopScan(); }

void ThumbnailBatchScanner::startScan(
    const std::string &directory, bool forceRegenerate,
    std::function<void(const Progress &)> progressCallback) {
  if (running) {
    std::cerr << "[ThumbnailBatchScanner] Scan already in progress"
              << std::endl;
    return;
  }
  if (!fs::exists(directory) || !fs::is_directory(directory)) {
    std::cerr << "[ThumbnailBatchScanner] Directory not found: " << directory
              << std::endl;
    return;
  }
  running = true;
  stopRequested = false;
  {
    std::lock_guard<std::mutex> lock(progressMutex);
    progress = Progress{};
    progress.isRunning = true;
  }
  scanThread = std::make_unique<std::thread>(
      &ThumbnailBatchScanner::scanThreadFunc, this, directory, forceRegenerate,
      progressCallback);
}

void ThumbnailBatchScanner::stopScan() {
  stopRequested = true;
  if (scanThread && scanThread->joinable()) {
    scanThread->join();
  }
  running = false;
}

ThumbnailBatchScanner::Progress ThumbnailBatchScanner::getProgress() const {
  std::lock_guard<std::mutex> lock(progressMutex);
  return progress;
}

void ThumbnailBatchScanner::scanThreadFunc(
    const std::string &directory, bool forceRegenerate,
    std::function<void(const Progress &)> progressCallback) {
  std::cout << "[ThumbnailBatchScanner] Starting scan of: " << directory
            << std::endl;
  std::vector<fs::path> videoFiles;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(directory)) {
      if (stopRequested) {
        std::cout << "[ThumbnailBatchScanner] Scan stopped by user"
                  << std::endl;
        goto cleanup;
      }
      if (entry.is_regular_file() && isVideoFile(entry.path())) {
        videoFiles.push_back(entry.path());
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[ThumbnailBatchScanner] Error scanning directory: "
              << e.what() << std::endl;
    goto cleanup;
  }
  {
    std::lock_guard<std::mutex> lock(progressMutex);
    progress.totalFiles = static_cast<int>(videoFiles.size());
    progress.isRunning = true;
  }
  std::cout << "[ThumbnailBatchScanner] Found " << videoFiles.size()
            << " video files" << std::endl;
  for (const auto &videoPath : videoFiles) {
    if (stopRequested) {
      std::cout << "[ThumbnailBatchScanner] Scan stopped by user" << std::endl;
      break;
    }
    {
      std::lock_guard<std::mutex> lock(progressMutex);
      progress.currentFile = videoPath.string();
      progress.processedFiles++;
    }
    if (shouldProcessFile(videoPath, forceRegenerate)) {
      VideoIntegrityChecker::Status status =
          VideoIntegrityChecker::check(videoPath.string());
      if (status == VideoIntegrityChecker::Status::Valid) {
        if (processVideo(videoPath)) {
          std::lock_guard<std::mutex> lock(progressMutex);
          progress.successfulThumbnails++;
        } else {
          std::lock_guard<std::mutex> lock(progressMutex);
          progress.failedFiles++;
          std::cerr << "[ThumbnailBatchScanner] Failed to create thumbnail: "
                    << videoPath << std::endl;
        }
      } else {
        std::lock_guard<std::mutex> lock(progressMutex);
        progress.failedFiles++;
        std::cerr << "[ThumbnailBatchScanner] Invalid video file: " << videoPath
                  << " (status: "
                  << VideoIntegrityChecker::statusToString(status) << ")"
                  << std::endl;
      }
    }
    if (progressCallback) {
      Progress copy;
      {
        std::lock_guard<std::mutex> lock(progressMutex);
        copy = progress;
      }
      progressCallback(copy);
    }
  }
cleanup:
  {
    std::lock_guard<std::mutex> lock(progressMutex);
    progress.isRunning = false;
    progress.currentFile.clear();
  }
  running = false;
  std::cout << "[ThumbnailBatchScanner] Scan completed: "
            << progress.successfulThumbnails << " thumbnails created, "
            << progress.failedFiles << " failed" << std::endl;
}

bool ThumbnailBatchScanner::isVideoFile(const fs::path &path) const {
  static const std::vector<std::string> videoExtensions = {
      ".mp4",  ".avi", ".mkv",  ".mov",  ".wmv",  ".flv",
      ".webm", ".m4v", ".mpg",  ".mpeg", ".3gp",  ".ogv",
      ".ts",   ".mts", ".m2ts", ".divx", ".xvid", ".vob"};
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return std::any_of(
      videoExtensions.begin(), videoExtensions.end(),
      [&ext](const std::string &validExt) { return ext == validExt; });
}

bool ThumbnailBatchScanner::shouldProcessFile(const fs::path &path,
                                              bool forceRegenerate) const {
  if (forceRegenerate) {
    return true;
  }
  return !database.imageExists(path.string());
}

bool ThumbnailBatchScanner::processVideo(const fs::path &path) {
  auto frame = thumbnailer.extractFrame(path, 300.0);
  if (!frame.has_value()) {
    frame = thumbnailer.extractFrame(path, 60.0);
    if (!frame.has_value()) {
      return false;
    }
  }
  std::vector<unsigned char> buffer = std::move(frame.value());
  if (buffer.empty()) {
    return false;
  }
  ImageData imageData;
  imageData.data = std::move(buffer);
  imageData.mimeType = "image/jpeg";
  return database.saveImage(path.string(), imageData);
}
