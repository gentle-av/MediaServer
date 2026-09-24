#include "MusicDatabase.h"
#include <filesystem>
#include <mutex>
#include <sqlite3.h>

class MusicDatabase::Impl {
public:
  std::mutex databaseMutex;
  explicit Impl(const std::string &dbPath) : dbPath_(dbPath), db(nullptr) {}
  ~Impl() {
    if (db)
      sqlite3_close(db);
  }

  bool init() {
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
      return false;
    }
    const char *encodingQuery = "PRAGMA encoding = \"UTF-8\";";
    char *errorText = nullptr;
    sqlite3_exec(db, encodingQuery, nullptr, nullptr, &errorText);
    const char *tableStructureQuery = R"(
            CREATE TABLE IF NOT EXISTS music_files (
                file_path TEXT PRIMARY KEY,
                title TEXT,
                artist TEXT,
                album TEXT,
                duration INTEGER,
                track INTEGER,
                year INTEGER,
                genre TEXT,
                last_scan TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE TABLE IF NOT EXISTS album_art (
                file_path TEXT PRIMARY KEY,
                art_data BLOB,
                mime_type TEXT,
                FOREIGN KEY(file_path) REFERENCES music_files(file_path) ON DELETE CASCADE
            );
        )";
    if (sqlite3_exec(db, tableStructureQuery, nullptr, nullptr, &errorText) !=
        SQLITE_OK) {
      sqlite3_free(errorText);
      return false;
    }
    return true;
  }

  sqlite3 *getDb() { return db; }

private:
  std::string dbPath_;
  sqlite3 *db;
};

MusicDatabase::MusicDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}

MusicDatabase::~MusicDatabase() = default;

bool MusicDatabase::init() { return pImpl->init(); }

void MusicDatabase::close() { pImpl.reset(); }

bool MusicDatabase::addFile(const std::string &filePath,
                            const MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sqlQuery =
      "INSERT OR REPLACE INTO music_files (file_path, title, artist, album, "
      "duration, track, year, genre) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statementHandle, 1, filePath.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(statementHandle, 2, metadata.title.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(statementHandle, 3, metadata.artist.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(statementHandle, 4, metadata.album.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(statementHandle, 5, metadata.duration);
    sqlite3_bind_int(statementHandle, 6, metadata.track);
    sqlite3_bind_int(statementHandle, 7, metadata.year);
    sqlite3_bind_text(statementHandle, 8, metadata.genre.c_str(), -1,
                      SQLITE_TRANSIENT);
    bool isInserted = (sqlite3_step(statementHandle) == SQLITE_DONE);
    sqlite3_finalize(statementHandle);
    return isInserted;
  }
  sqlite3_finalize(statementHandle);
  return false;
}

bool MusicDatabase::removeFile(const std::string &filePath) {
  const char *sql = "DELETE FROM music_files WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
  }
  sqlite3_finalize(stmt);
  return false;
}

bool MusicDatabase::fileExists(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sqlQuery = "SELECT 1 FROM music_files WHERE file_path = ?";
  sqlite3_stmt *statementHandle;
  bool isFound = false;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statementHandle, 1, filePath.c_str(), -1,
                      SQLITE_TRANSIENT);
    isFound = (sqlite3_step(statementHandle) == SQLITE_ROW);
    sqlite3_finalize(statementHandle);
  }
  return isFound;
}

bool MusicDatabase::getMetadata(const std::string &filePath,
                                MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sqlQuery =
      "SELECT file_path, title, artist, album, duration, track, "
      "year, genre FROM music_files WHERE file_path = ?";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statementHandle, 1, filePath.c_str(), -1,
                      SQLITE_TRANSIENT);
    if (sqlite3_step(statementHandle) == SQLITE_ROW) {
      metadata.filePath = reinterpret_cast<const char *>(
                              sqlite3_column_text(statementHandle, 0))
                              ?: "";
      metadata.title = reinterpret_cast<const char *>(
                           sqlite3_column_text(statementHandle, 1))
                           ?: "";
      metadata.artist = reinterpret_cast<const char *>(
                            sqlite3_column_text(statementHandle, 2))
                            ?: "";
      metadata.album = reinterpret_cast<const char *>(
                           sqlite3_column_text(statementHandle, 3))
                           ?: "";
      metadata.duration = sqlite3_column_int(statementHandle, 4);
      metadata.track = sqlite3_column_int(statementHandle, 5);
      metadata.year = sqlite3_column_int(statementHandle, 6);
      metadata.genre = reinterpret_cast<const char *>(
                           sqlite3_column_text(statementHandle, 7))
                           ?: "";
      sqlite3_finalize(statementHandle);
      return true;
    }
  }
  sqlite3_finalize(statementHandle);
  return false;
}

std::vector<std::string> MusicDatabase::getAllFilePaths() {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<std::string> filePaths;
  const char *sqlQuery = "SELECT file_path FROM music_files";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    while (sqlite3_step(statementHandle) == SQLITE_ROW) {
      const char *filePathText = reinterpret_cast<const char *>(
          sqlite3_column_text(statementHandle, 0));
      if (filePathText) {
        filePaths.push_back(filePathText);
      }
    }
    sqlite3_finalize(statementHandle);
  }
  return filePaths;
}

bool MusicDatabase::saveAlbumArt(const std::string &filePath,
                                 const std::vector<char> &albumArt) {
  std::string mimeType = "image/jpeg";
  if (albumArt.size() >= 8) {
    if (albumArt[0] == (char)0xFF && albumArt[1] == (char)0xD8)
      mimeType = "image/jpeg";
    else if (albumArt[0] == (char)0x89 && albumArt[1] == (char)0x50)
      mimeType = "image/png";
    else if (albumArt[0] == (char)0x47 && albumArt[1] == (char)0x49)
      mimeType = "image/gif";
  }
  const char *sql = "INSERT OR REPLACE INTO album_art (file_path, art_data, "
                    "mime_type) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 2, albumArt.data(), albumArt.size(),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, mimeType.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

AlbumArtData MusicDatabase::getAlbumArt(const std::string &filePath) {
  AlbumArtData result;
  const char *sql =
      "SELECT art_data, mime_type FROM album_art WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const void *data = sqlite3_column_blob(stmt, 0);
      int size = sqlite3_column_bytes(stmt, 0);
      if (data && size > 0) {
        result.data.assign(static_cast<const char *>(data),
                           static_cast<const char *>(data) + size);
        const char *mime =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        if (mime)
          result.mimeType = mime;
      }
    }
    sqlite3_finalize(stmt);
  }
  return result;
}

bool MusicDatabase::removeAlbumArt(const std::string &filePath) {
  const char *sql = "DELETE FROM album_art WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<MusicMetadata>
MusicDatabase::getTracksByArtistRaw(const std::string &artistName) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<MusicMetadata> tracks;
  const char *sqlQuery =
      "SELECT file_path, title, artist, album, duration, track, year, genre "
      "FROM music_files WHERE artist = ? ORDER BY album, track";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statementHandle, 1, artistName.c_str(), -1,
                      SQLITE_TRANSIENT);
    tracks.reserve(100);
    while (sqlite3_step(statementHandle) == SQLITE_ROW) {
      tracks.emplace_back();
      MusicMetadata &metadata = tracks.back();
      const char *filePathText =
          (const char *)sqlite3_column_text(statementHandle, 0);
      const char *titleText =
          (const char *)sqlite3_column_text(statementHandle, 1);
      const char *artistText =
          (const char *)sqlite3_column_text(statementHandle, 2);
      const char *albumText =
          (const char *)sqlite3_column_text(statementHandle, 3);
      const char *genreText =
          (const char *)sqlite3_column_text(statementHandle, 7);
      if (filePathText)
        metadata.filePath.assign(filePathText);
      if (titleText)
        metadata.title.assign(titleText);
      if (artistText)
        metadata.artist.assign(artistText);
      if (albumText)
        metadata.album.assign(albumText);
      if (genreText)
        metadata.genre.assign(genreText);
      metadata.duration = sqlite3_column_int(statementHandle, 4);
      metadata.track = sqlite3_column_int(statementHandle, 5);
      metadata.year = sqlite3_column_int(statementHandle, 6);
    }
    sqlite3_finalize(statementHandle);
  }
  return tracks;
}

std::vector<MusicMetadata>
MusicDatabase::getTracksByAlbumRaw(const std::string &albumName,
                                   const std::string &artistName) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<MusicMetadata> tracks;
  std::string sqlQuery =
      "SELECT file_path, title, artist, album, duration, track, "
      "year, genre FROM music_files WHERE album = ?";
  if (!artistName.empty() && artistName != "Unknown") {
    sqlQuery += " AND artist = ?";
  }
  sqlQuery += " ORDER BY track";
  sqlite3_stmt *statementHandle = nullptr;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery.c_str(), -1, &statementHandle,
                         nullptr) != SQLITE_OK) {
    return tracks;
  }
  sqlite3_bind_text(statementHandle, 1, albumName.c_str(), -1,
                    SQLITE_TRANSIENT);
  if (!artistName.empty() && artistName != "Unknown") {
    std::string artistParam = artistName;
    if (artistParam == "Unknown Artist") {
      artistParam = "Unknown";
    }
    sqlite3_bind_text(statementHandle, 2, artistParam.c_str(), -1,
                      SQLITE_TRANSIENT);
  }
  tracks.reserve(100);
  while (sqlite3_step(statementHandle) == SQLITE_ROW) {
    tracks.emplace_back();
    MusicMetadata &metadata = tracks.back();
    const char *filePathText =
        (const char *)sqlite3_column_text(statementHandle, 0);
    const char *titleText =
        (const char *)sqlite3_column_text(statementHandle, 1);
    const char *artistText =
        (const char *)sqlite3_column_text(statementHandle, 2);
    const char *albumText =
        (const char *)sqlite3_column_text(statementHandle, 3);
    const char *genreText =
        (const char *)sqlite3_column_text(statementHandle, 7);
    if (filePathText)
      metadata.filePath.assign(filePathText);
    if (titleText)
      metadata.title.assign(titleText);
    if (artistText)
      metadata.artist.assign(artistText);
    if (albumText)
      metadata.album.assign(albumText);
    if (genreText)
      metadata.genre.assign(genreText);
    metadata.duration = sqlite3_column_int(statementHandle, 4);
    metadata.track = sqlite3_column_int(statementHandle, 5);
    metadata.year = sqlite3_column_int(statementHandle, 6);
  }
  sqlite3_finalize(statementHandle);
  return tracks;
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicDatabase::getAlbumsRaw(const std::string &artistFilter) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<std::tuple<std::string, std::string, std::string>> albums;
  std::string sqlQuery =
      "SELECT album, artist, MAX(year) FROM music_files WHERE "
      "album != '' AND album IS NOT NULL AND album != 'Unknown'";
  if (!artistFilter.empty()) {
    sqlQuery += " AND artist = ?";
  }
  sqlQuery += " GROUP BY album, artist ORDER BY artist, album";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery.c_str(), -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    if (!artistFilter.empty()) {
      sqlite3_bind_text(statementHandle, 1, artistFilter.c_str(), -1,
                        SQLITE_TRANSIENT);
    }
    while (sqlite3_step(statementHandle) == SQLITE_ROW) {
      std::string album = reinterpret_cast<const char *>(
                              sqlite3_column_text(statementHandle, 0))
                              ?: "";
      std::string artist = reinterpret_cast<const char *>(
                               sqlite3_column_text(statementHandle, 1))
                               ?: "";
      std::string year = std::to_string(sqlite3_column_int(statementHandle, 2));
      albums.emplace_back(album, artist, year);
    }
    sqlite3_finalize(statementHandle);
  }
  return albums;
}

std::string
MusicDatabase::getFilePathByAlbumRaw(const std::string &albumName,
                                     const std::string &artistName) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::string sqlQuery = "SELECT m.file_path FROM music_files m "
                         "JOIN album_art a ON m.file_path = a.file_path "
                         "WHERE m.album LIKE ? AND m.artist LIKE ? AND "
                         "LENGTH(a.art_data) > 0 LIMIT 1";
  sqlite3_stmt *statementHandle = nullptr;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery.c_str(), -1, &statementHandle,
                         nullptr) != SQLITE_OK) {
    return "";
  }
  std::string albumParam = albumName;
  std::string artistParam = artistName;
  if (artistParam == "Unknown Artist" || artistParam.empty() ||
      artistParam == "Unknown") {
    artistParam = "%";
  }
  if (albumParam.find('%') == std::string::npos) {
    albumParam = "%" + albumParam + "%";
  }
  if (artistParam != "%" && artistParam.find('%') == std::string::npos) {
    artistParam = "%" + artistParam + "%";
  }
  sqlite3_bind_text(statementHandle, 1, albumParam.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(statementHandle, 2, artistParam.c_str(), -1,
                    SQLITE_TRANSIENT);
  std::string pathText;
  if (sqlite3_step(statementHandle) == SQLITE_ROW) {
    const char *filePathText =
        reinterpret_cast<const char *>(sqlite3_column_text(statementHandle, 0));
    if (filePathText) {
      pathText = filePathText;
    }
  }
  sqlite3_finalize(statementHandle);
  return pathText;
}

std::vector<std::string> MusicDatabase::getArtistsRaw() {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<std::string> artists;
  if (!pImpl || !pImpl->getDb()) {
    return artists;
  }
  const char *sqlQuery =
      "SELECT DISTINCT artist FROM music_files WHERE artist != "
      "'' AND artist != 'Unknown' ORDER BY artist";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlQuery, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    while (sqlite3_step(statementHandle) == SQLITE_ROW) {
      const char *artistText = reinterpret_cast<const char *>(
          sqlite3_column_text(statementHandle, 0));
      if (artistText) {
        artists.push_back(artistText);
      }
    }
    sqlite3_finalize(statementHandle);
  }
  return artists;
}
