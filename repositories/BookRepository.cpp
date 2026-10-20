#include "BookRepository.h"
#include "../database/BookDatabase.h"
#include "../services/books/Fb2MetadataExtractor.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

BookRepository::BookRepository(std::shared_ptr<BookDatabase> db)
    : db(std::move(db)) {}

bool BookRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTtl;
}

std::vector<Book> BookRepository::getAllBooks() {
  std::shared_lock lock(mutex);
  if (allBooksCache.data && !isExpired(allBooksCache.timestamp)) {
    return *allBooksCache.data;
  }
  lock.unlock();
  refreshAllBooksCache();
  std::shared_lock readLock(mutex);
  std::cout << "[BookRepository::getAllBooks] there is "
            << allBooksCache.data.get()->size()
            << " books in allBooksCache array\n";
  return *allBooksCache.data;
}

void BookRepository::refreshAllBooksCache() const {
  auto books = db->getAllBooks();
  std::cout << "[BookRepository::refreshBooksCache] books size is "
            << books.size() << '\n';
  std::unique_lock lock(mutex);
  allBooksCache.data =
      std::make_shared<const std::vector<Book>>(std::move(books));
  allBooksCache.timestamp = std::chrono::steady_clock::now();
}

std::optional<Book> BookRepository::getBook(const std::string &filePath) {
  auto books = getAllBooks();
  for (const auto &book : books) {
    if (book.filePath == filePath)
      return book;
  }
  return std::nullopt;
}

bool BookRepository::addBook(const Book &book) {
  bool success = db->addBook(book);
  if (success)
    invalidateAll();
  return success;
}

bool BookRepository::removeBook(const std::string &filePath) {
  bool success = db->removeBook(filePath);
  if (success) {
    {
      std::unique_lock lock(mutex);
      invalidateAllLocked();
    }
    eventBus.publish("book_removed", filePath);
    eventBus.publish("cacheinvalidated");
  }
  return success;
}

void BookRepository::invalidateAll() {
  std::unique_lock lock(mutex);
  invalidateAllLocked();
  eventBus.publish("cacheinvalidated");
}

void BookRepository::invalidateAllLocked() {
  allBooksCache.data.reset();
  allBooksCache.timestamp = std::chrono::steady_clock::time_point{};
}

size_t BookRepository::getCacheSize() const {
  std::shared_lock lock(mutex);
  return allBooksCache.data ? 1 : 0;
}

bool BookRepository::isBookFile(const std::string &path) const {
  auto ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".fb2";
}

std::shared_future<bool> BookRepository::scanBooksDirectoryAsync(
    const std::string &booksDir,
    std::function<void(int total, int processed)> progressCallback) {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid()) {
    auto status = scanFuture.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::timeout) {
      return std::shared_future<bool>();
    }
  }
  {
    std::unique_lock cacheLock(mutex);
    invalidateAllLocked();
  }
  stopSource = std::stop_source{};
  scanPromise = std::promise<bool>();
  scanFuture = scanPromise.get_future().share();
  scanThread = std::jthread([this, booksDir,
                             progressCallback](std::stop_token stopToken) {
    bool success = doScanBooksDirectory(booksDir, progressCallback, stopToken);
    scanPromise.set_value(success);
  });
  return scanFuture;
}

bool BookRepository::doScanBooksDirectory(
    const std::string &booksDir,
    std::function<void(int total, int processed)> progressCallback,
    std::stop_token stopToken) {
  try {
    if (stopToken.stop_requested())
      return false;
    if (!fs::exists(booksDir) || !fs::is_directory(booksDir))
      return false;
    std::vector<std::string> allFiles;
    for (const auto &entry : fs::recursive_directory_iterator(booksDir)) {
      if (stopToken.stop_requested())
        return false;
      if (entry.is_regular_file() && isBookFile(entry.path().string())) {
        allFiles.push_back(entry.path().string());
      }
    }
    auto existingPaths = db->getAllBookPaths();
    std::unordered_set<std::string> existingSet(existingPaths.begin(),
                                                existingPaths.end());
    std::unordered_set<std::string> foundSet;
    int total = static_cast<int>(allFiles.size());
    int processed = 0;
    std::vector<Book> batch;
    constexpr int BATCH_SIZE = 50;
    for (const auto &path : allFiles) {
      if (stopToken.stop_requested())
        return false;
      foundSet.insert(path);
      if (existingSet.find(path) == existingSet.end()) {
        Book book;
        if (Fb2MetadataExtractor::extractFull(path, book)) {
          batch.push_back(std::move(book));
          if (batch.size() >= BATCH_SIZE) {
            db->addBooksBatch(batch);
            batch.clear();
          }
        }
      }
      processed++;
      if (progressCallback)
        progressCallback(total, processed);
    }
    if (!batch.empty()) {
      db->addBooksBatch(batch);
      batch.clear();
    }
    std::vector<std::string> removedPaths;
    for (const auto &path : existingSet) {
      if (stopToken.stop_requested())
        return false;
      if (foundSet.find(path) == foundSet.end()) {
        if (db->removeBook(path)) {
          removedPaths.push_back(path);
        }
      }
    }
    {
      std::unique_lock lock(mutex);
      invalidateAllLocked();
    }
    for (const auto &path : removedPaths) {
      eventBus.publish("book_removed", path);
    }
    eventBus.publish("books_scan_completed");
    return true;
  } catch (...) {
    return false;
  }
}

void BookRepository::cancelScan() { stopSource.request_stop(); }

bool BookRepository::isScanning() const {
  std::lock_guard<std::mutex> lock(scanMutex);
  return scanFuture.valid() &&
         scanFuture.wait_for(std::chrono::milliseconds(0)) ==
             std::future_status::timeout;
}

void BookRepository::waitForScan() {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid())
    scanFuture.wait();
}
