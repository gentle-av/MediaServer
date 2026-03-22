#include "database/MusicDatabase.h"
#include "services/AlbumArtExtractor.h"
#include "services/MusicMetadataExtractor.h"
#include <algorithm>
#include <chrono>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sqlite3.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <thread>

namespace fs = std::filesystem;

static std::time_t toTimeT(const fs::file_time_type &ftime) {
  auto duration = ftime.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  return seconds.count();
}

class MusicDatabase::Impl {
public:
  Impl(const std::string &dbPath) : dbPath_(dbPath), db_(nullptr) {}

  ~Impl() {
    if (db_)
      sqlite3_close(db_);
  }

  bool initialize() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK)
      return false;
    const char *createTables = R"(
            CREATE TABLE IF NOT EXISTS artists (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL
            );
            CREATE TABLE IF NOT EXISTS albums (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                artist_id INTEGER,
                name TEXT NOT NULL,
                path TEXT UNIQUE,
                album_art BLOB,
                album_art_mime TEXT,
                FOREIGN KEY(artist_id) REFERENCES artists(id)
            );
            CREATE TABLE IF NOT EXISTS tracks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                album_id INTEGER,
                path TEXT UNIQUE NOT NULL,
                title TEXT,
                track_number INTEGER,
                duration INTEGER,
                bitrate INTEGER,
                sample_rate INTEGER,
                file_size INTEGER,
                modified_time INTEGER,
                FOREIGN KEY(album_id) REFERENCES albums(id)
            );
            CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album_id);
            CREATE INDEX IF NOT EXISTS idx_tracks_path ON tracks(path);
            CREATE INDEX IF NOT EXISTS idx_albums_name ON albums(name);
            CREATE INDEX IF NOT EXISTS idx_artists_name ON artists(name);
        )";
    char *errMsg = nullptr;
    rc = sqlite3_exec(db_, createTables, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
      sqlite3_free(errMsg);
      return false;
    }
    return true;
  }

  void
  scanDirectory(const std::string &rootPath,
                std::function<void(const std::string &)> progressCallback) {
    std::lock_guard<std::mutex> lock(scanMutex_);
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath))
      return;
    for (const auto &entry : fs::recursive_directory_iterator(rootPath)) {
      if (!entry.is_regular_file())
        continue;
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (!isAudioFile(ext))
        continue;
      std::time_t modTime = toTimeT(entry.last_write_time());
      if (isTrackCached(entry.path().string(), modTime))
        continue;
      if (progressCallback)
        progressCallback(entry.path().filename().string());
      TrackMetadata metadata;
      metadata.path = entry.path().string();
      metadata.fileSize = entry.file_size();
      metadata.modifiedTime = modTime;
      extractMetadata(metadata);
      saveTrack(metadata);
    }
  }

  std::vector<std::tuple<std::string, std::string, std::string>>
  getAlbums(const std::string &artistFilter) {
    std::vector<std::tuple<std::string, std::string, std::string>> result;
    std::string sql =
        "SELECT artists.name, albums.name, albums.path FROM albums LEFT JOIN "
        "artists ON albums.artist_id = artists.id";
    if (!artistFilter.empty())
      sql += " WHERE artists.name LIKE ?";
    sql += " ORDER BY artists.name, albums.name";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
      return result;
    if (!artistFilter.empty()) {
      std::string pattern = "%" + artistFilter + "%";
      sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      std::string album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      std::string path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      result.emplace_back(artist, album, path);
    }
    sqlite3_finalize(stmt);
    return result;
  }

  std::vector<std::string> getArtists() {
    std::vector<std::string> result;
    const char *sql = "SELECT name FROM artists ORDER BY name";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
      return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      result.push_back(
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return result;
  }

  std::vector<TrackMetadata> getTracksByAlbum(const std::string &albumName,
                                              const std::string &artistName) {
    std::vector<TrackMetadata> result;
    const char *sql =
        "SELECT tracks.path, tracks.title, artists.name, albums.name, "
        "tracks.track_number, tracks.duration, tracks.bitrate, "
        "tracks.sample_rate, tracks.file_size, tracks.modified_time FROM "
        "tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN "
        "artists ON albums.artist_id = artists.id WHERE albums.name = ? AND "
        "artists.name = ? ORDER BY tracks.track_number";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
      return result;
    sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, artistName.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      TrackMetadata track;
      track.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      track.title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      track.artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      track.album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      track.trackNumber = sqlite3_column_int(stmt, 4);
      track.duration = sqlite3_column_int(stmt, 5);
      track.bitrate = sqlite3_column_int(stmt, 6);
      track.sampleRate = sqlite3_column_int(stmt, 7);
      track.fileSize = sqlite3_column_int64(stmt, 8);
      track.modifiedTime = sqlite3_column_int64(stmt, 9);
      result.push_back(track);
    }
    sqlite3_finalize(stmt);
    return result;
  }

  AlbumArtData getAlbumArt(const std::string &path) {
    AlbumArtData result;
    const char *sql =
        "SELECT album_art, album_art_mime FROM albums WHERE path = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
      return result;
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const void *data = sqlite3_column_blob(stmt, 0);
      int size = sqlite3_column_bytes(stmt, 0);
      if (data && size > 0) {
        result.data.resize(size);
        memcpy(result.data.data(), data, size);
        result.mimeType =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      }
    }
    sqlite3_finalize(stmt);
    return result;
  }

  bool isTrackCached(const std::string &path, std::time_t modTime) {
    const char *sql = "SELECT modified_time FROM tracks WHERE path = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
      return false;
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    bool cached = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      std::time_t cachedTime = sqlite3_column_int64(stmt, 0);
      cached = (cachedTime == modTime);
    }
    sqlite3_finalize(stmt);
    return cached;
  }

  void saveTrack(const TrackMetadata &track) {
    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    int artistId = getOrCreateArtist(track.artist);
    int albumId = getOrCreateAlbum(track.album, track.artist, artistId,
                                   track.path, track.albumArt);
    const char *insertTrack =
        "INSERT OR REPLACE INTO tracks (album_id, path, title, track_number, "
        "duration, bitrate, sample_rate, file_size, modified_time) VALUES (?, "
        "?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_, insertTrack, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, albumId);
    sqlite3_bind_text(stmt, 2, track.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, track.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, track.trackNumber);
    sqlite3_bind_int(stmt, 5, track.duration);
    sqlite3_bind_int(stmt, 6, track.bitrate);
    sqlite3_bind_int(stmt, 7, track.sampleRate);
    sqlite3_bind_int64(stmt, 8, track.fileSize);
    sqlite3_bind_int64(stmt, 9, track.modifiedTime);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
  }

  void forceRescan(const std::string &rootPath) {
    const char *deleteTracks = "DELETE FROM tracks WHERE path LIKE ?";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_, deleteTracks, -1, &stmt, nullptr);
    std::string pattern = rootPath + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    scanDirectory(rootPath, nullptr);
  }

private:
  bool isAudioFile(const std::string &ext) {
    std::vector<std::string> exts = {".mp3", ".flac", ".wav", ".aac",
                                     ".ogg", ".m4a",  ".wma", ".opus"};
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
  }

  void extractMetadata(TrackMetadata &metadata) {
    TagLib::FileRef file(metadata.path.c_str());
    if (!file.isNull()) {
      TagLib::Tag *tag = file.tag();
      if (tag) {
        metadata.title = tag->title().to8Bit(true);
        metadata.artist = tag->artist().to8Bit(true);
        metadata.album = tag->album().to8Bit(true);
        metadata.trackNumber = tag->track();
        if (file.audioProperties()) {
          metadata.duration = file.audioProperties()->lengthInSeconds();
          metadata.bitrate = file.audioProperties()->bitrate();
          metadata.sampleRate = file.audioProperties()->sampleRate();
        }
      }
    }
    if (metadata.artist.empty())
      metadata.artist = "Unknown Artist";
    if (metadata.album.empty())
      metadata.album = "Unknown Album";
    if (metadata.title.empty())
      metadata.title = fs::path(metadata.path).stem().string();
    auto albumArt = AlbumArtExtractor::extractAlbumArt(metadata.path);
    if (albumArt && !albumArt->data.empty()) {
      metadata.albumArt.data = std::move(albumArt->data);
      metadata.albumArt.mimeType = std::move(albumArt->mimeType);
    }
  }

  int getOrCreateArtist(const std::string &artistName) {
    const char *selectSql = "SELECT id FROM artists WHERE name = ?";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, artistName.c_str(), -1, SQLITE_STATIC);
    int artistId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      artistId = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (artistId == -1) {
      const char *insertSql = "INSERT INTO artists (name) VALUES (?)";
      sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr);
      sqlite3_bind_text(stmt, 1, artistName.c_str(), -1, SQLITE_STATIC);
      sqlite3_step(stmt);
      artistId = sqlite3_last_insert_rowid(db_);
      sqlite3_finalize(stmt);
    }
    return artistId;
  }

  int getOrCreateAlbum(const std::string &albumName,
                       const std::string &artistName, int artistId,
                       const std::string &trackPath,
                       const AlbumArtData &albumArt) {
    const char *selectSql =
        "SELECT id FROM albums WHERE name = ? AND artist_id = ?";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, artistId);
    int albumId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      albumId = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (albumId == -1) {
      std::string albumPath = fs::path(trackPath).parent_path().string();
      const char *insertSql =
          "INSERT INTO albums (artist_id, name, path, album_art, "
          "album_art_mime) VALUES (?, ?, ?, ?, ?)";
      sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr);
      sqlite3_bind_int(stmt, 1, artistId);
      sqlite3_bind_text(stmt, 2, albumName.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 3, albumPath.c_str(), -1, SQLITE_STATIC);
      if (!albumArt.data.empty()) {
        sqlite3_bind_blob(stmt, 4, albumArt.data.data(), albumArt.data.size(),
                          SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, albumArt.mimeType.c_str(), -1,
                          SQLITE_STATIC);
      } else {
        sqlite3_bind_null(stmt, 4);
        sqlite3_bind_null(stmt, 5);
      }
      sqlite3_step(stmt);
      albumId = sqlite3_last_insert_rowid(db_);
      sqlite3_finalize(stmt);
    }
    return albumId;
  }

  std::string dbPath_;
  sqlite3 *db_;
  std::mutex scanMutex_;
};

MusicDatabase::MusicDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}

MusicDatabase::~MusicDatabase() = default;

bool MusicDatabase::initialize() { return pImpl->initialize(); }

void MusicDatabase::scanDirectory(
    const std::string &rootPath,
    std::function<void(const std::string &)> progressCallback) {
  pImpl->scanDirectory(rootPath, progressCallback);
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicDatabase::getAlbums(const std::string &artistFilter) {
  return pImpl->getAlbums(artistFilter);
}

std::vector<std::string> MusicDatabase::getArtists() {
  return pImpl->getArtists();
}

std::vector<TrackMetadata>
MusicDatabase::getTracksByAlbum(const std::string &albumName,
                                const std::string &artistName) {
  return pImpl->getTracksByAlbum(albumName, artistName);
}

AlbumArtData MusicDatabase::getAlbumArt(const std::string &path) {
  return pImpl->getAlbumArt(path);
}

bool MusicDatabase::isTrackCached(const std::string &path,
                                  std::time_t modTime) {
  return pImpl->isTrackCached(path, modTime);
}

void MusicDatabase::saveTrack(const TrackMetadata &track) {
  pImpl->saveTrack(track);
}

void MusicDatabase::forceRescan(const std::string &rootPath) {
  pImpl->forceRescan(rootPath);
}
