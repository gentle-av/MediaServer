#include "MusicDatabase.h"
#include <iostream>
#include <sqlite3.h>

class MusicDatabase::Impl {
public:
  explicit Impl(const std::string &dbPath) : dbPath_(dbPath), db_(nullptr) {}
  ~Impl() {
    if (db_)
      sqlite3_close(db_);
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

MusicDatabase::MusicDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}
MusicDatabase::~MusicDatabase() = default;

bool MusicDatabase::init() { return pImpl->init(); }
void MusicDatabase::close() { pImpl.reset(); }

std::vector<std::string> MusicDatabase::getAllFiles() {
  std::vector<std::string> files;
  const char *sql = "SELECT file_path FROM music_files";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (path)
        files.push_back(path);
    }
    sqlite3_finalize(stmt);
  }
  return files;
}

bool MusicDatabase::addFile(const std::string &filePath,
                            const MusicMetadata &metadata) {
  const char *sql =
      "INSERT OR REPLACE INTO music_files (file_path, title, artist, album, "
      "duration, track, year, genre) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, metadata.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, metadata.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, metadata.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, metadata.duration);
    sqlite3_bind_int(stmt, 6, metadata.track);
    sqlite3_bind_int(stmt, 7, metadata.year);
    sqlite3_bind_text(stmt, 8, metadata.genre.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
  }
  sqlite3_finalize(stmt);
  return false;
}

bool MusicDatabase::removeFile(const std::string &filePath) {
  const char *sql = "DELETE FROM music_files WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
  }
  sqlite3_finalize(stmt);
  return false;
}

bool MusicDatabase::getMetadata(const std::string &filePath,
                                MusicMetadata &metadata) {
  const char *sql = "SELECT file_path, title, artist, album, duration, track, "
                    "year, genre FROM music_files WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      metadata.filePath =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)) ?: "";
      metadata.title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)) ?: "";
      metadata.artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)) ?: "";
      metadata.album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)) ?: "";
      metadata.duration = sqlite3_column_int(stmt, 4);
      metadata.track = sqlite3_column_int(stmt, 5);
      metadata.year = sqlite3_column_int(stmt, 6);
      metadata.genre =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7)) ?: "";
      sqlite3_finalize(stmt);
      return true;
    }
  }
  sqlite3_finalize(stmt);
  return false;
}

bool MusicDatabase::fileExists(const std::string &filePath) {
  const char *sql = "SELECT 1 FROM music_files WHERE file_path = ?";
  sqlite3_stmt *stmt;
  bool exists = false;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }
  return exists;
}

bool MusicDatabase::saveAlbumArt(const std::string &filePath,
                                 const std::vector<char> &albumArt) {
  std::string mimeType = "image/jpeg";
  if (albumArt.size() >= 8) {
    if (albumArt[0] == 0xFF && albumArt[1] == 0xD8) {
      mimeType = "image/jpeg";
    } else if (albumArt[0] == 0x89 && albumArt[1] == 0x50 &&
               albumArt[2] == 0x4E && albumArt[3] == 0x47) {
      mimeType = "image/png";
    } else if (albumArt[0] == 0x47 && albumArt[1] == 0x49 &&
               albumArt[2] == 0x46) {
      mimeType = "image/gif";
    }
  }
  const char *sql = "INSERT OR REPLACE INTO album_art (file_path, art_data, "
                    "mime_type) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, albumArt.data(), albumArt.size(),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, mimeType.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
  }
  sqlite3_finalize(stmt);
  return false;
}

AlbumArtData MusicDatabase::getAlbumArt(const std::string &filePath) {
  AlbumArtData result;
  const char *sql =
      "SELECT art_data, mime_type FROM album_art WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
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

std::vector<std::string> MusicDatabase::getArtists() {
  std::vector<std::string> artists;
  const char *sql = "SELECT DISTINCT artist FROM music_files WHERE artist != "
                    "'' AND artist != 'Unknown' ORDER BY artist";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (artist)
        artists.push_back(artist);
    }
    sqlite3_finalize(stmt);
  }
  return artists;
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicDatabase::getAlbums(const std::string &artistFilter) {
  std::vector<std::tuple<std::string, std::string, std::string>> albums;
  std::string sql = "SELECT album, artist, MAX(year) FROM music_files WHERE "
                    "album != '' AND album IS NOT NULL AND album != 'Unknown'";
  if (!artistFilter.empty())
    sql += " AND artist = ?";
  sql += " GROUP BY album, artist ORDER BY artist, album";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql.c_str(), -1, &stmt, nullptr) ==
      SQLITE_OK) {
    if (!artistFilter.empty())
      sqlite3_bind_text(stmt, 1, artistFilter.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)) ?: "";
      std::string artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)) ?: "";
      std::string year = std::to_string(sqlite3_column_int(stmt, 2));
      albums.emplace_back(album, artist, year);
    }
    sqlite3_finalize(stmt);
  }
  return albums;
}

std::vector<MusicMetadata>
MusicDatabase::getTracksByAlbum(const std::string &albumName,
                                const std::string &artistName) {
  std::vector<MusicMetadata> tracks;
  std::string sql = "SELECT file_path, title, artist, album, duration, track, "
                    "year, genre FROM music_files WHERE album = ?";
  if (!artistName.empty() && artistName != "Unknown") {
    sql += " AND artist = ?";
  }
  sql += " ORDER BY track";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(pImpl->db(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return tracks;
  }
  sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_TRANSIENT);
  if (!artistName.empty() && artistName != "Unknown") {
    sqlite3_bind_text(stmt, 2, artistName.c_str(), -1, SQLITE_TRANSIENT);
  }
  tracks.reserve(100);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tracks.emplace_back();
    MusicMetadata &meta = tracks.back();
    const char *path = (const char *)sqlite3_column_text(stmt, 0);
    const char *title = (const char *)sqlite3_column_text(stmt, 1);
    const char *artist = (const char *)sqlite3_column_text(stmt, 2);
    const char *album = (const char *)sqlite3_column_text(stmt, 3);
    const char *genre = (const char *)sqlite3_column_text(stmt, 7);
    if (path)
      meta.filePath.assign(path);
    if (title)
      meta.title.assign(title);
    if (artist)
      meta.artist.assign(artist);
    if (album)
      meta.album.assign(album);
    if (genre)
      meta.genre.assign(genre);
    meta.duration = sqlite3_column_int(stmt, 4);
    meta.track = sqlite3_column_int(stmt, 5);
    meta.year = sqlite3_column_int(stmt, 6);
  }
  sqlite3_finalize(stmt);
  return tracks;
}

void MusicDatabase::scanDirectory(
    const std::string &path,
    std::function<void(const std::string &)> callback) {
  if (callback)
    callback(path);
}

void MusicDatabase::forceRescan(const std::string &rootPath) {}

std::vector<MusicMetadata>
MusicDatabase::getTracksByArtist(const std::string &artistName) {
  std::vector<MusicMetadata> tracks;
  const char *sql =
      "SELECT file_path, title, artist, album, duration, track, year, genre "
      "FROM music_files WHERE artist = ? ORDER BY album, track";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, artistName.c_str(), -1, SQLITE_TRANSIENT);
    tracks.reserve(100);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      tracks.emplace_back();
      MusicMetadata &meta = tracks.back();
      const char *path = (const char *)sqlite3_column_text(stmt, 0);
      const char *title = (const char *)sqlite3_column_text(stmt, 1);
      const char *artist = (const char *)sqlite3_column_text(stmt, 2);
      const char *album = (const char *)sqlite3_column_text(stmt, 3);
      const char *genre = (const char *)sqlite3_column_text(stmt, 7);
      if (path)
        meta.filePath.assign(path);
      if (title)
        meta.title.assign(title);
      if (artist)
        meta.artist.assign(artist);
      if (album)
        meta.album.assign(album);
      if (genre)
        meta.genre.assign(genre);
      meta.duration = sqlite3_column_int(stmt, 4);
      meta.track = sqlite3_column_int(stmt, 5);
      meta.year = sqlite3_column_int(stmt, 6);
    }
    sqlite3_finalize(stmt);
  }
  return tracks;
}

std::string MusicDatabase::getFilePathByAlbum(const std::string &albumName,
                                              const std::string &artistName) {
  std::string sql = "SELECT file_path FROM music_files WHERE album = ?";
  if (!artistName.empty() && artistName != "Unknown") {
    sql += " AND artist = ?";
  }
  sql += " LIMIT 1";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(pImpl->db(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return "";
  }
  sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_TRANSIENT);
  if (!artistName.empty() && artistName != "Unknown") {
    sqlite3_bind_text(stmt, 2, artistName.c_str(), -1, SQLITE_TRANSIENT);
  }
  std::string result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *path =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (path)
      result = path;
  }
  sqlite3_finalize(stmt);
  return result;
}
