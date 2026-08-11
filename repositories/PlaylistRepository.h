#pragma once

#include "../database/PlaylistDatabase.h"
#include "../models/Playlist.h"
#include "MusicRepository.h"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class PlaylistRepository {
public:
  explicit PlaylistRepository(std::unique_ptr<PlaylistDatabase> db,
                              std::unique_ptr<MusicRepository> musicRepo);
  ~PlaylistRepository() = default;

  std::vector<std::string> getPlaylistNames() const;
  std::shared_ptr<const Playlist> getPlaylist(const std::string &name) const;
  bool hasPlaylist(const std::string &name) const;
  bool savePlaylist(const std::string &name, const Playlist &playlist);
  bool savePlaylist(const std::string &name, Playlist &&playlist);
  bool deletePlaylist(const std::string &name);
  bool renamePlaylist(const std::string &oldName, const std::string &newName);
  bool createPlaylistFromArtist(const std::string &name,
                                const std::string &artistName);
  bool createPlaylistFromAlbum(const std::string &name,
                               const std::string &albumName,
                               const std::string &artistName = "");
  bool createPlaylistFromSearch(const std::string &name,
                                const std::string &query);
  bool importPlaylist(const std::string &filePath);
  bool exportPlaylist(const std::string &name,
                      const std::string &filePath) const;
  void invalidateAll();
  void setTTL(std::chrono::seconds ttl) { defaultTTL = ttl; }
  size_t getCacheSize() const;
  std::shared_future<bool> scanPlaylistDirectoryAsync(
      const std::string &playlistDir,
      std::function<void(int total, int processed)> progressCallback = nullptr);
  void cancelScan();
  bool isScanning() const;
  void waitForScan();

private:
  struct CachedPlaylist {
    std::shared_ptr<const Playlist> data;
    std::chrono::steady_clock::time_point timestamp;
  };

  struct CachedPlaylistList {
    std::vector<std::string> data;
    std::chrono::steady_clock::time_point timestamp;
  };

  bool isExpired(const std::chrono::steady_clock::time_point &timestamp) const;
  void refreshPlaylistListCache() const;
  void invalidateAllLocked();

  std::shared_ptr<const Playlist>
  loadPlaylistFromDB(const std::string &name) const;
  std::vector<std::string> loadAllPlaylistNamesFromDB() const;
  bool savePlaylistToDB(const std::string &name, const Playlist &playlist);
  bool deletePlaylistFromDB(const std::string &name);

  bool isPlaylistFile(const std::string &path) const;
  bool doScanPlaylistDirectory(
      const std::string &playlistDir,
      std::function<void(int total, int processed)> progressCallback,
      std::stop_token stopToken);

  std::unique_ptr<PlaylistDatabase> db;
  std::unique_ptr<MusicRepository> musicRepo;
  std::chrono::seconds defaultTTL{60};
  mutable std::shared_mutex mutex;
  mutable CachedPlaylistList playlistListCache;
  mutable std::unordered_map<std::string, CachedPlaylist> playlistCache;

  std::jthread scanThread;
  std::stop_source stopSource;
  std::promise<bool> scanPromise;
  std::shared_future<bool> scanFuture;
  mutable std::mutex scanMutex;
};
