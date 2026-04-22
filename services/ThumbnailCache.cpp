#include "ThumbnailCache.h"
#include "ThumbnailExtractor.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

ThumbnailCache &ThumbnailCache::getInstance() {
  static ThumbnailCache instance;
  return instance;
}

ThumbnailCache::~ThumbnailCache() { shutdown(); }

bool ThumbnailCache::init(const std::string &dbPath) {
  std::lock_guard<std::mutex> lock(mutex_);
  dbPath_ = dbPath;
  int rc = sqlite3_open(dbPath.c_str(), &db_);
  if (rc != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
    return false;
  }
  sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);
  return createTable();
}

bool ThumbnailCache::createTable() {
  const char *sql = R"(
        CREATE TABLE IF NOT EXISTS thumbnails (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            file_hash TEXT NOT NULL,
            thumbnail_data TEXT NOT NULL,
            file_size INTEGER,
            last_modified INTEGER,
            created_at INTEGER,
            accessed_at INTEGER,
            width INTEGER DEFAULT 320,
            quality INTEGER DEFAULT 85,
            UNIQUE(file_path, width, quality)
        );
        CREATE INDEX IF NOT EXISTS idx_file_path ON thumbnails(file_path);
        CREATE INDEX IF NOT EXISTS idx_last_modified ON thumbnails(last_modified);
        CREATE INDEX IF NOT EXISTS idx_accessed_at ON thumbnails(accessed_at);
    )";
  char *errMsg = nullptr;
  int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << errMsg << std::endl;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

std::string ThumbnailCache::computeHash(const std::string &videoPath) {
  std::hash<std::string> hasher;
  size_t hash = hasher(videoPath);
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(16) << hash;
  return ss.str();
}

bool ThumbnailCache::getFromCache(const std::string &videoPath, int width,
                                  int quality, std::string &thumbnail) {
  fs::path path(videoPath);
  if (!fs::exists(path)) {
    return false;
  }
  auto fileSize = fs::file_size(path);
  auto lastModified = fs::last_write_time(path);
  auto lastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(
                              lastModified.time_since_epoch())
                              .count();
  sqlite3_stmt *stmt;
  const char *selectSql = R"(
        SELECT thumbnail_data, last_modified, file_size
        FROM thumbnails
        WHERE file_path = ? AND width = ? AND quality = ?
    )";
  int rc = sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, videoPath.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, width);
  sqlite3_bind_int(stmt, 3, quality);
  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    thumbnail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    int64_t cachedLastModified = sqlite3_column_int64(stmt, 1);
    int64_t cachedFileSize = sqlite3_column_int64(stmt, 2);
    if (cachedLastModified == lastModifiedTime &&
        cachedFileSize == static_cast<int64_t>(fileSize)) {
      found = true;
      const char *updateAccessSql =
          "UPDATE thumbnails SET accessed_at = ? WHERE file_path = ? AND width "
          "= ? AND quality = ?";
      sqlite3_stmt *updateStmt;
      if (sqlite3_prepare_v2(db_, updateAccessSql, -1, &updateStmt, nullptr) ==
          SQLITE_OK) {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        sqlite3_bind_int64(updateStmt, 1, now);
        sqlite3_bind_text(updateStmt, 2, videoPath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(updateStmt, 3, width);
        sqlite3_bind_int(updateStmt, 4, quality);
        sqlite3_step(updateStmt);
        sqlite3_finalize(updateStmt);
      }
    }
  }
  sqlite3_finalize(stmt);
  return found;
}

void ThumbnailCache::saveToCache(const std::string &videoPath,
                                 const std::string &thumbnail, int width,
                                 int quality) {
  fs::path path(videoPath);
  auto fileSize =
      fs::exists(path) ? static_cast<int64_t>(fs::file_size(path)) : 0;
  auto lastModified = fs::exists(path)
                          ? std::chrono::duration_cast<std::chrono::seconds>(
                                fs::last_write_time(path).time_since_epoch())
                                .count()
                          : 0;
  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  const char *insertSql = R"(
        INSERT OR REPLACE INTO thumbnails
        (file_path, file_hash, thumbnail_data, file_size, last_modified, created_at, accessed_at, width, quality)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  std::string hash = computeHash(videoPath);
  sqlite3_bind_text(stmt, 1, videoPath.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, thumbnail.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 4, fileSize);
  sqlite3_bind_int64(stmt, 5, lastModified);
  sqlite3_bind_int64(stmt, 6, now);
  sqlite3_bind_int64(stmt, 7, now);
  sqlite3_bind_int(stmt, 8, width);
  sqlite3_bind_int(stmt, 9, quality);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void ThumbnailCache::batchInsert(
    const std::vector<std::tuple<std::string, std::string, int, int>> &items) {
  if (items.empty())
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
  for (const auto &item : items) {
    const auto &[videoPath, thumbnail, width, quality] = item;
    saveToCache(videoPath, thumbnail, width, quality);
  }
  sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::string ThumbnailCache::getThumbnail(const std::string &videoPath,
                                         int width, int quality) {
  std::string thumbnail;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (getFromCache(videoPath, width, quality, thumbnail)) {
      return thumbnail;
    }
  }
  std::vector<uint8_t> imageData;
  if (!ThumbnailExtractor::generateRawThumbnail(videoPath, width, quality,
                                                imageData)) {
    return "";
  }
  thumbnail = ThumbnailExtractor::base64Encode(imageData);
  if (!thumbnail.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    saveToCache(videoPath, thumbnail, width, quality);
  }
  return thumbnail;
}

void ThumbnailCache::clearCache() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_) {
    sqlite3_exec(db_, "DELETE FROM thumbnails", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "VACUUM", nullptr, nullptr, nullptr);
  }
}

void ThumbnailCache::cleanupOldEntries(int daysOld) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_)
    return;
  auto cutoff = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count() -
                (daysOld * 86400);
  std::string sql =
      "DELETE FROM thumbnails WHERE accessed_at < " + std::to_string(cutoff);
  sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
}

void ThumbnailCache::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_) {
    sqlite3_exec(db_, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr,
                 nullptr);
    sqlite3_close(db_);
    db_ = nullptr;
  }
}
