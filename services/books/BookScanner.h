#pragma once

#include "../../database/BookDatabase.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct BookScanStatus {
  std::atomic<bool> inProgress{false};
  std::atomic<int> totalFiles{0};
  std::atomic<int> processedFiles{0};
  std::atomic<int> addedFiles{0};
  std::atomic<int> errorCount{0};
  std::chrono::steady_clock::time_point startTime;
  std::chrono::steady_clock::time_point lastScanTime;
  void reset() {
    inProgress = false;
    totalFiles = 0;
    processedFiles = 0;
    addedFiles = 0;
    errorCount = 0;
  }
};

class BookScanner {
public:
  BookScanner(BookDatabase &db, const std::string &booksDir);
  ~BookScanner();
  void scanNewFiles(bool skipExistingInDb = true);
  void removeMissingFiles();
  void forceRescan(std::function<void()> onComplete = nullptr);
  bool isInProgress() const { return status.inProgress.load(); }
  const BookScanStatus &getStatus() const { return status; }

private:
  bool isBookFile(const std::string &path);
  std::vector<std::string> scanBooksDirectory();
  void processFile(const std::string &path, bool addToDb);
  void doRescan(std::function<void()> onComplete);
  bool shouldProcessFile(const std::string &path, bool skipExistingInDb);

  BookDatabase &db;
  std::string booksDir;
  BookScanStatus status;
  std::mutex mutex;
  std::unique_ptr<std::thread> rescanThread;
};
