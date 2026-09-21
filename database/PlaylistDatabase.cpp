#include "PlaylistDatabase.h"
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <sqlite3.h>
#include <vector>

class PlaylistDatabase::Impl {
public:
  explicit Impl(const std::string &dbPath)
      : dbPath_(dbPath), dbHandle(nullptr) {}

  ~Impl() {
    if (dbHandle)
      sqlite3_close(dbHandle);
  }

  bool init() {
    if (sqlite3_open(dbPath_.c_str(), &dbHandle) != SQLITE_OK) {
      return false;
    }
    const char *encodingQuery = "PRAGMA encoding = \"UTF-8\";";
    char *errorText = nullptr;
    sqlite3_exec(dbHandle, encodingQuery, nullptr, nullptr, &errorText);
    const char *schemaQuery = R"(
            CREATE TABLE IF NOT EXISTS playlists (
                playlist_name TEXT PRIMARY KEY,
                current_index INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE TABLE IF NOT EXISTS playlist_tracks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                playlist_name TEXT NOT NULL,
                position INTEGER NOT NULL,
                file_path TEXT NOT NULL,
                title TEXT,
                artist TEXT,
                album TEXT,
                duration INTEGER DEFAULT 0,
                track INTEGER DEFAULT 0,
                year INTEGER DEFAULT 0,
                genre TEXT,
                FOREIGN KEY(playlist_name) REFERENCES playlists(playlist_name) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_playlist_tracks_name ON playlist_tracks(playlist_name);
            CREATE INDEX IF NOT EXISTS idx_playlist_tracks_position ON playlist_tracks(playlist_name, position);
        )";
    if (sqlite3_exec(dbHandle, schemaQuery, nullptr, nullptr, &errorText) !=
        SQLITE_OK) {
      sqlite3_free(errorText);
      return false;
    }
    return true;
  }

  sqlite3 *db() { return dbHandle; }
  std::mutex databaseMutex;

private:
  std::string dbPath_;
  sqlite3 *dbHandle;
};

PlaylistDatabase::PlaylistDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}

PlaylistDatabase::~PlaylistDatabase() = default;

bool PlaylistDatabase::init() { return pImpl->init(); }

void PlaylistDatabase::close() { pImpl.reset(); }

bool PlaylistDatabase::savePlaylist(const std::string &playlistName,
                                    const Playlist &playlist) {
  return savePlaylist(playlistName, playlist.getAllTracks());
}

bool PlaylistDatabase::savePlaylist(const std::string &playlistName,
                                    const std::vector<MusicMetadata> &tracks) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  char *errorText = nullptr;
  if (sqlite3_exec(pImpl->db(), "BEGIN TRANSACTION", nullptr, nullptr,
                   &errorText) != SQLITE_OK) {
    sqlite3_free(errorText);
    return false;
  }
  bool isSuccess = true;
  const char *playlistSql =
      "INSERT OR REPLACE INTO playlists (playlist_name, current_index, "
      "updated_at) VALUES (?, ?, CURRENT_TIMESTAMP)";
  sqlite3_stmt *statementHandle;
  if (sqlite3_prepare_v2(pImpl->db(), playlistSql, -1, &statementHandle,
                         nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statementHandle, 1, playlistName.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(statementHandle, 2, 0);
    if (sqlite3_step(statementHandle) != SQLITE_DONE) {
      isSuccess = false;
    }
    sqlite3_finalize(statementHandle);
  } else {
    isSuccess = false;
  }
  if (isSuccess) {
    const char *deleteSql =
        "DELETE FROM playlist_tracks WHERE playlist_name = ?";
    if (sqlite3_prepare_v2(pImpl->db(), deleteSql, -1, &statementHandle,
                           nullptr) == SQLITE_OK) {
      sqlite3_bind_text(statementHandle, 1, playlistName.c_str(), -1,
                        SQLITE_TRANSIENT);
      if (sqlite3_step(statementHandle) != SQLITE_DONE) {
        isSuccess = false;
      }
      sqlite3_finalize(statementHandle);
    } else {
      isSuccess = false;
    }
  }
  if (isSuccess) {
    const char *insertSql =
        "INSERT INTO playlist_tracks (playlist_name, position, file_path, "
        "title, artist, album, duration, track, year, genre) VALUES (?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(pImpl->db(), insertSql, -1, &statementHandle,
                           nullptr) == SQLITE_OK) {
      for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        const auto &trackMetadata = tracks[trackIndex];
        sqlite3_bind_text(statementHandle, 1, playlistName.c_str(), -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_int(statementHandle, 2, static_cast<int>(trackIndex));
        sqlite3_bind_text(statementHandle, 3, trackMetadata.filePath.c_str(),
                          -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statementHandle, 4, trackMetadata.title.c_str(), -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(statementHandle, 5, trackMetadata.artist.c_str(), -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(statementHandle, 6, trackMetadata.album.c_str(), -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_int(statementHandle, 7, trackMetadata.duration);
        sqlite3_bind_int(statementHandle, 8, trackMetadata.track);
        sqlite3_bind_int(statementHandle, 9, trackMetadata.year);
        sqlite3_bind_text(statementHandle, 10, trackMetadata.genre.c_str(), -1,
                          SQLITE_TRANSIENT);
        if (sqlite3_step(statementHandle) != SQLITE_DONE) {
          isSuccess = false;
          break;
        }
        sqlite3_reset(statementHandle);
      }
      sqlite3_finalize(statementHandle);
    } else {
      isSuccess = false;
    }
  }
  if (isSuccess) {
    sqlite3_exec(pImpl->db(), "COMMIT", nullptr, nullptr, &errorText);
  } else {
    sqlite3_exec(pImpl->db(), "ROLLBACK", nullptr, nullptr, &errorText);
    if (errorText) {
      sqlite3_free(errorText);
    }
  }
  return isSuccess;
}

std::optional<Playlist>
PlaylistDatabase::loadPlaylist(const std::string &playlistName) {
  Playlist playlist(std::vector<MusicMetadata>{});
  if (loadPlaylist(playlistName, playlist))
    return playlist;
  return std::nullopt;
}

bool PlaylistDatabase::loadPlaylist(const std::string &playlistName,
                                    Playlist &playlist) {
  std::vector<MusicMetadata> tracks;
  int currentIndex = 0;
  const char *playlistSQL =
      "SELECT current_index FROM playlists WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), playlistSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      currentIndex = sqlite3_column_int(stmt, 0);
    } else {
      sqlite3_finalize(stmt);
      return false;
    }
    sqlite3_finalize(stmt);
  } else {
    return false;
  }
  const char *trackSQL =
      "SELECT file_path, title, artist, album, duration, track, year, genre "
      "FROM playlist_tracks WHERE playlist_name = ? ORDER BY position";
  if (sqlite3_prepare_v2(pImpl->db(), trackSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      MusicMetadata meta;
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      const char *title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      const char *artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      const char *album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      const char *genre =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
      meta.filePath = path ? path : "";
      meta.title = title ? title : "";
      meta.artist = artist ? artist : "";
      meta.album = album ? album : "";
      meta.duration = sqlite3_column_int(stmt, 4);
      meta.track = sqlite3_column_int(stmt, 5);
      meta.year = sqlite3_column_int(stmt, 6);
      meta.genre = genre ? genre : "";
      tracks.push_back(meta);
    }
    sqlite3_finalize(stmt);
    playlist = Playlist(tracks);
    playlist.setCurrentIndex(currentIndex);
    return true;
  }
  return false;
}

bool PlaylistDatabase::deletePlaylist(const std::string &playlistName) {
  const char *sql = "DELETE FROM playlists WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
  }
  return false;
}

bool PlaylistDatabase::playlistExists(const std::string &playlistName) const {
  const char *sql = "SELECT 1 FROM playlists WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  bool exists = false;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }
  return exists;
}

std::vector<std::string> PlaylistDatabase::getAllPlaylistNames() const {
  std::vector<std::string> names;
  const char *sql =
      "SELECT playlist_name FROM playlists ORDER BY playlist_name";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *name =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (name)
        names.push_back(name);
    }
    sqlite3_finalize(stmt);
  }
  return names;
}

int PlaylistDatabase::getTrackCount(const std::string &playlistName) const {
  const char *sql =
      "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  int count = 0;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
      count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
  }
  return count;
}

bool PlaylistDatabase::addTrackToPlaylist(const std::string &playlistName,
                                          const MusicMetadata &track) {
  const char *countSQL =
      "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  int position = 0;
  if (sqlite3_prepare_v2(pImpl->db(), countSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
      position = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
  } else {
    return false;
  }
  const char *insertSQL =
      "INSERT INTO playlist_tracks (playlist_name, position, file_path, title, "
      "artist, album, duration, track, year, genre) VALUES (?, ?, ?, ?, ?, ?, "
      "?, ?, ?, ?)";
  if (sqlite3_prepare_v2(pImpl->db(), insertSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, position);
    sqlite3_bind_text(stmt, 3, track.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, track.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, track.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, track.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, track.duration);
    sqlite3_bind_int(stmt, 8, track.track);
    sqlite3_bind_int(stmt, 9, track.year);
    sqlite3_bind_text(stmt, 10, track.genre.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (success) {
      const char *updateSQL = "UPDATE playlists SET updated_at = "
                              "CURRENT_TIMESTAMP WHERE playlist_name = ?";
      if (sqlite3_prepare_v2(pImpl->db(), updateSQL, -1, &stmt, nullptr) ==
          SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }
    }
    return success;
  }
  return false;
}

bool PlaylistDatabase::addTrackToPlaylist(const std::string &playlistName,
                                          const std::string &filePath) {
  MusicMetadata meta;
  meta.filePath = filePath;
  return addTrackToPlaylist(playlistName, meta);
}

bool PlaylistDatabase::removeTrackFromPlaylist(const std::string &playlistName,
                                               int index) {
  const char *deleteSQL =
      "DELETE FROM playlist_tracks WHERE playlist_name = ? AND position = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), deleteSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, index);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (success) {
      const char *updateSQL = "UPDATE playlist_tracks SET position = position "
                              "- 1 WHERE playlist_name = ? AND position > ?";
      if (sqlite3_prepare_v2(pImpl->db(), updateSQL, -1, &stmt, nullptr) ==
          SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, index);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }
      const char *updateTimeSQL = "UPDATE playlists SET updated_at = "
                                  "CURRENT_TIMESTAMP WHERE playlist_name = ?";
      if (sqlite3_prepare_v2(pImpl->db(), updateTimeSQL, -1, &stmt, nullptr) ==
          SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }
    }
    return success;
  }
  return false;
}

bool PlaylistDatabase::clearPlaylist(const std::string &playlistName) {
  const char *sql = "DELETE FROM playlist_tracks WHERE playlist_name = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (success) {
      const char *resetSQL =
          "UPDATE playlists SET current_index = 0, updated_at = "
          "CURRENT_TIMESTAMP WHERE playlist_name = ?";
      if (sqlite3_prepare_v2(pImpl->db(), resetSQL, -1, &stmt, nullptr) ==
          SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }
    }
    return success;
  }
  return false;
}

bool PlaylistDatabase::shufflePlaylist(const std::string &playlistName) {
  std::vector<MusicMetadata> tracks;
  const char *selectSQL =
      "SELECT file_path, title, artist, album, duration, track, year, genre "
      "FROM playlist_tracks WHERE playlist_name = ? ORDER BY position";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->db(), selectSQL, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, playlistName.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      MusicMetadata meta;
      const char *path =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      const char *title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      const char *artist =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      const char *album =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      const char *genre =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
      meta.filePath = path ? path : "";
      meta.title = title ? title : "";
      meta.artist = artist ? artist : "";
      meta.album = album ? album : "";
      meta.duration = sqlite3_column_int(stmt, 4);
      meta.track = sqlite3_column_int(stmt, 5);
      meta.year = sqlite3_column_int(stmt, 6);
      meta.genre = genre ? genre : "";
      tracks.push_back(meta);
    }
    sqlite3_finalize(stmt);
  } else {
    return false;
  }
  if (tracks.size() < 2)
    return true;
  static std::random_device rd;
  static std::mt19937 g(rd());
  std::shuffle(tracks.begin(), tracks.end(), g);
  return savePlaylist(playlistName, tracks);
}
