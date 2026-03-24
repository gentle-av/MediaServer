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
    char *errMsg = nullptr;
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
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, metadata.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, metadata.artist.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, metadata.album.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, metadata.duration);
    sqlite3_bind_int(stmt, 6, metadata.track);
    sqlite3_bind_int(stmt, 7, metadata.year);
    sqlite3_bind_text(stmt, 8, metadata.genre.c_str(), -1, SQLITE_STATIC);
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
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
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
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
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
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }
  return exists;
}

bool MusicDatabase::saveAlbumArt(const std::string &filePath,
                                 const std::vector<char> &albumArt) {
  const char *sql = "INSERT OR REPLACE INTO album_art (file_path, art_data, "
                    "mime_type) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, albumArt.data(), albumArt.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, "image/jpeg", -1, SQLITE_STATIC);
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
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
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
  std::string sql = "SELECT DISTINCT album, artist, MAX(year) FROM music_files "
                    "WHERE album != ''";
  if (!artistFilter.empty())
    sql += " AND artist = ?";
  sql += " GROUP BY album, artist ORDER BY artist, album";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql.c_str(), -1, &stmt, nullptr) ==
      SQLITE_OK) {
    if (!artistFilter.empty())
      sqlite3_bind_text(stmt, 1, artistFilter.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *albumCol =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      const char *artistCol =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      int yearVal = sqlite3_column_int(stmt, 2);
      std::string album = albumCol ? albumCol : "";
      std::string artist = artistCol ? artistCol : "";
      std::string year = yearVal > 0 ? std::to_string(yearVal) : "";
      if (!album.empty()) {
        albums.emplace_back(album, artist, year);
      }
    }
    sqlite3_finalize(stmt);
  }
  return albums;
}

std::vector<MusicMetadata>
MusicDatabase::getTracksByAlbum(const std::string &albumName,
                                const std::string &artistName) {
  std::vector<MusicMetadata> tracks;
  if (albumName.empty()) {
    return tracks;
  }
  try {
    std::string sql = "SELECT file_path, title, artist, album, duration, "
                      "track, year, genre FROM music_files WHERE album = ?";
    if (!artistName.empty()) {
      sql += " AND artist = ?";
    }
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(pImpl->db(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      return tracks;
    }
    rc = sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
      sqlite3_finalize(stmt);
      return tracks;
    }
    int paramIndex = 2;
    if (!artistName.empty()) {
      rc = sqlite3_bind_text(stmt, paramIndex, artistName.c_str(), -1,
                             SQLITE_STATIC);
      if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return tracks;
      }
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (!path || strlen(path) == 0) {
        continue;
      }
      const char *title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      const char *artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      const char *album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      int duration = sqlite3_column_int(stmt, 4);
      int trackNum = sqlite3_column_int(stmt, 5);
      int year = sqlite3_column_int(stmt, 6);
      const char *genre =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
      tracks.emplace_back();
      MusicMetadata &meta = tracks.back();
      meta.filePath = std::string(path);
      if (title)
        meta.title = std::string(title);
      if (artist)
        meta.artist = std::string(artist);
      if (album)
        meta.album = std::string(album);
      meta.duration = duration;
      meta.track = trackNum;
      meta.year = year;
      if (genre)
        meta.genre = std::string(genre);
    }
    sqlite3_finalize(stmt);
  } catch (const std::exception &e) {
  }
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
    sqlite3_bind_text(stmt, 1, artistName.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      MusicMetadata meta;
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (path && strlen(path) > 0) {
        meta.filePath = std::string(path);
        const char *title =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        if (title)
          meta.title = std::string(title);
        const char *artist =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        if (artist)
          meta.artist = std::string(artist);
        const char *album =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        if (album)
          meta.album = std::string(album);
        meta.duration = sqlite3_column_int(stmt, 4);
        meta.track = sqlite3_column_int(stmt, 5);
        meta.year = sqlite3_column_int(stmt, 6);
        const char *genre =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        if (genre)
          meta.genre = std::string(genre);
        tracks.push_back(std::move(meta));
      }
    }
    sqlite3_finalize(stmt);
  }
  return tracks;
}
