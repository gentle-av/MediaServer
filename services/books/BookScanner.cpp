#include "BookScanner.h"
#include "Fb2MetadataExtractor.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

BookScanner::BookScanner(BookDatabase &db, const std::string &booksDir)
    : db(db), booksDir(booksDir) {}

BookScanner::~BookScanner() {
  if (rescanThread && rescanThread->joinable()) {
    rescanThread->join();
  }
}

bool BookScanner::isBookFile(const std::string &path) {
  auto ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".fb2";
}

std::vector<std::string> BookScanner::scanBooksDirectory() {
  std::vector<std::string> bookFiles;
  if (!fs::exists(booksDir))
    return bookFiles;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(booksDir)) {
      if (entry.is_regular_file() && isBookFile(entry.path().string())) {
        bookFiles.push_back(entry.path().string());
      }
    }
  } catch (...) {
  }
  return bookFiles;
}

bool BookScanner::shouldProcessFile(const std::string &path,
                                    bool skipExistingInDb) {
  if (!skipExistingInDb)
    return true;
  return !db.bookExists(path);
}

void BookScanner::processFile(const std::string &path, bool addToDb) {
  Book book;
  if (Fb2MetadataExtractor::extractMetadata(path, book)) {
    if (addToDb && db.addBook(book)) {
      status.addedFiles++;
    }
  } else {
    status.errorCount++;
  }
}

void BookScanner::scanNewFiles(bool skipExistingInDb) {
  if (status.inProgress)
    return;
  std::thread([this, skipExistingInDb]() {
    std::lock_guard<std::mutex> lock(mutex);
    status.reset();
    status.inProgress = true;
    status.startTime = std::chrono::steady_clock::now();
    auto bookFiles = scanBooksDirectory();
    status.totalFiles = static_cast<int>(bookFiles.size());
    status.processedFiles = 0;
    for (const auto &path : bookFiles) {
      if (shouldProcessFile(path, skipExistingInDb)) {
        processFile(path, true);
      }
      status.processedFiles++;
    }
    status.inProgress = false;
    status.lastScanTime = std::chrono::steady_clock::now();
  }).detach();
}

void BookScanner::removeMissingFiles() {
  auto dbFiles = db.getAllBookPaths();
  for (const auto &path : dbFiles) {
    if (!fs::exists(path)) {
      db.removeBook(path);
    }
  }
}

void BookScanner::forceRescan(std::function<void()> onComplete) {
  if (status.inProgress) {
    if (onComplete)
      onComplete();
    return;
  }
  doRescan(onComplete);
}

void BookScanner::doRescan(std::function<void()> onComplete) {
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
      auto bookFiles = scanBooksDirectory();
      status.totalFiles = static_cast<int>(bookFiles.size());
      std::unordered_set<std::string> foundFiles;
      status.addedFiles = 0;
      status.errorCount = 0;
      status.processedFiles = 0;
      for (const auto &path : bookFiles) {
        foundFiles.insert(path);
        processFile(path, true);
        status.processedFiles++;
      }
      auto dbFiles = db.getAllBookPaths();
      for (const auto &path : dbFiles) {
        if (foundFiles.find(path) == foundFiles.end()) {
          db.removeBook(path);
        }
      }
    } catch (...) {
      status.errorCount++;
    }
    status.inProgress = false;
    status.lastScanTime = std::chrono::steady_clock::now();
    if (onComplete)
      onComplete();
  });
}
