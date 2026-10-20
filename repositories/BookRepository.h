#pragma once

#include "../database/BookDatabase.h"
#include "../events/EventBus.h"
#include "../models/Book.h"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

class BookRepository {
public:
  explicit BookRepository(std::shared_ptr<BookDatabase> db);
  ~BookRepository() = default;

  std::vector<Book> getAllBooks();
  std::optional<Book> getBook(const std::string &filePath);
  bool addBook(const Book &book);
  bool removeBook(const std::string &filePath);

  void invalidateAll();
  void setTTL(std::chrono::seconds ttl) { defaultTtl = ttl; }
  size_t getCacheSize() const;

  std::shared_future<bool> scanBooksDirectoryAsync(
      const std::string &booksDir,
      std::function<void(int total, int processed)> progressCallback = nullptr);
  void cancelScan();
  bool isScanning() const;
  void waitForScan();

  EventBus &getEventBus() { return eventBus; }
  void subscribe(const std::string &event, EventBus::EventCallback callback) {
    eventBus.subscribe(event, callback);
  }

private:
  struct CachedBooks {
    std::shared_ptr<const std::vector<Book>> data;
    std::chrono::steady_clock::time_point timestamp;
  };

  bool isExpired(const std::chrono::steady_clock::time_point &timestamp) const;
  void refreshAllBooksCache() const;
  void invalidateAllLocked();
  bool isBookFile(const std::string &path) const;
  bool doScanBooksDirectory(
      const std::string &booksDir,
      std::function<void(int total, int processed)> progressCallback,
      std::stop_token stopToken);
  std::shared_ptr<BookDatabase> db;
  std::chrono::seconds defaultTtl{60};
  mutable std::shared_mutex mutex;
  mutable CachedBooks allBooksCache;
  std::jthread scanThread;
  std::stop_source stopSource;
  std::promise<bool> scanPromise;
  std::shared_future<bool> scanFuture;
  mutable std::mutex scanMutex;
  EventBus eventBus;
};
