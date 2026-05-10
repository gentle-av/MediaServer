#pragma once

#include <future>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <tuple>
#include <vector>

struct ThumbnailTask {
  std::string videoPath;
  int width;
  int quality;
  std::promise<std::string> promise;
};

class ThumbnailCache {
public:
  static ThumbnailCache &getInstance();
  bool init(const std::string &dbPath = "thumbnails.db");
  std::string getThumbnail(const std::string &videoPath, int width = 320,
                           int quality = 85);
  std::vector<std::pair<std::string, std::string>>
  getThumbnailsBatch(const std::vector<std::string> &videoPaths,
                     int width = 320, int quality = 85, size_t numThreads = 0);
  void clearCache();
  void cleanupOldEntries(int daysOld = 30);
  void shutdown();

private:
  ThumbnailCache() = default;
  ~ThumbnailCache();
  ThumbnailCache(const ThumbnailCache &) = delete;
  ThumbnailCache &operator=(const ThumbnailCache &) = delete;
  bool createTable();
  std::string generateThumbnailAndCache(const std::string &videoPath, int width,
                                        int quality);
  std::string computeHash(const std::string &videoPath);
  bool getFromCache(const std::string &videoPath, int width, int quality,
                    std::string &thumbnail);
  void saveToCache(const std::string &videoPath, const std::string &thumbnail,
                   int width, int quality);
  void batchInsert(
      const std::vector<std::tuple<std::string, std::string, int, int>> &items);
  sqlite3 *db_ = nullptr;
  std::mutex mutex_;
  std::string dbPath_;
  std::mutex batchMutex_;
  std::vector<std::tuple<std::string, std::string, int, int>> batchBuffer_;
};
