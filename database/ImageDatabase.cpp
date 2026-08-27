#include "ImageDatabase.h"
#include <iostream>
#include <sqlite3.h>

class ImageDatabase::Impl {
public:
  explicit Impl(const std::string &dbPath) : dbPath_(dbPath), db_(nullptr) {}

  ~Impl() {
    if (db_) {
      sqlite3_close(db_);
    }
  }

  bool init() {
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
      std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }
    const char *encodingSQL = "PRAGMA encoding = \"UTF-8\";";
    char *errMsg = nullptr;
    sqlite3_exec(db_, encodingSQL, nullptr, nullptr, &errMsg);
    const char *createTableSQL = R"(
            CREATE TABLE IF NOT EXISTS images (
                file_path TEXT PRIMARY KEY,
                image_data BLOB,
                mime_type TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        )";
    if (sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg) !=
        SQLITE_OK) {
      std::cerr << "SQL error: " << errMsg << std::endl;
      sqlite3_free(errMsg);
      return false;
    }
    return true;
  }

  sqlite3 *db() { return db_; }

private:
  std::string dbPath_;
  sqlite3 *db_;
};

ImageDatabase::ImageDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}

ImageDatabase::~ImageDatabase() = default;

bool ImageDatabase::init() { return pImpl->init(); }

void ImageDatabase::close() { pImpl.reset(); }

bool ImageDatabase::saveImage(const std::string &filePath,
                              const ImageData &image) {
  const char *sql = "INSERT OR REPLACE INTO images (file_path, image_data, "
                    "mime_type) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 2, image.data.data(), image.data.size(),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, image.mimeType.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::optional<ImageData> ImageDatabase::getImage(const std::string &filePath) {
  const char *sql =
      "SELECT image_data, mime_type FROM images WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ImageData result;
    const void *data = sqlite3_column_blob(stmt, 0);
    int size = sqlite3_column_bytes(stmt, 0);
    if (data && size > 0) {
      const unsigned char *bytes = static_cast<const unsigned char *>(data);
      result.data.assign(bytes, bytes + size);
      const char *mime =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      if (mime) {
        result.mimeType = mime;
      }
      sqlite3_finalize(stmt);
      return result;
    }
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

bool ImageDatabase::removeImage(const std::string &filePath) {
  const char *sql = "DELETE FROM images WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool ImageDatabase::imageExists(const std::string &filePath) {
  const char *sql = "SELECT 1 FROM images WHERE file_path = ?";
  sqlite3_stmt *stmt;
  bool exists = false;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }
  return exists;
}

std::vector<std::string> ImageDatabase::getAllPaths() {
  std::vector<std::string> paths;
  const char *sql = "SELECT file_path FROM images ORDER BY file_path";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (path) {
        paths.push_back(path);
      }
    }
    sqlite3_finalize(stmt);
  }
  return paths;
}
