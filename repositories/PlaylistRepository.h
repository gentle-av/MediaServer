#pragma once

#include "../database/PlaylistDatabase.h"
#include "../models/Playlist.h"
#include "../repositories/MusicRepository.h"
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class PlaylistRepository {
public:
  explicit PlaylistRepository(std::shared_ptr<PlaylistDatabase> db,
                              std::shared_ptr<MusicRepository> musicRepo);
  ~PlaylistRepository() = default;

  bool savePlaylist(const std::string &name, const Playlist &playlist);
  bool savePlaylist(const std::string &name, Playlist &&playlist);

  std::shared_ptr<const Playlist> loadPlaylist(const std::string &name) const;
  bool deletePlaylist(const std::string &name);
  bool renamePlaylist(const std::string &oldName, const std::string &newName);
  bool playlistExists(const std::string &name) const;

  std::vector<std::string> getAllPlaylistNames() const;
  int getTrackCount(const std::string &name) const;

  Playlist createFromArtist(const std::string &artistName) const;
  Playlist createFromAlbum(const std::string &albumName,
                           const std::string &artistName = "") const;
  Playlist createFromSearch(const std::string &query) const;
  Playlist createFromFilePaths(const std::vector<std::string> &paths) const;

  bool createAndSaveFromArtist(const std::string &name,
                               const std::string &artistName);
  bool createAndSaveFromAlbum(const std::string &name,
                              const std::string &albumName,
                              const std::string &artistName = "");
  bool createAndSaveFromSearch(const std::string &name,
                               const std::string &query);

  bool importFromFile(const std::string &filePath);
  bool exportToFile(const std::string &name, const std::string &filePath) const;

  void invalidateAll();
  void setTTL(std::chrono::seconds ttl) { defaultTtl = ttl; }
  size_t getCacheSize() const;

  std::shared_future<bool> scanDirectoryAsync(
      const std::string &playlistDir,
      std::function<void(int total, int processed)> progressCallback = nullptr);
  void cancelScan();
  bool isScanning() const;
  void waitForScan();

  void validateAllPlaylists();
  void validatePlaylist(const std::string &name);
  void onTrackRemoved(const std::string &filePath);

  MusicRepository *getMusicRepository() const { return musicRepo.get(); }

private:
  struct CachedPlaylistList {
    std::vector<std::string> data;
    std::chrono::steady_clock::time_point timestamp;
  };
  struct CachedPlaylist {
    std::shared_ptr<const Playlist> data;
    std::chrono::steady_clock::time_point timestamp;
  };

  bool isExpired(const std::chrono::steady_clock::time_point &timestamp) const;
  void refreshPlaylistListCache() const;
  void invalidateAllLocked();

  bool saveToDatabase(const std::string &name, const Playlist &playlist);
  std::shared_ptr<const Playlist>
  loadFromDatabase(const std::string &name) const;
  bool deleteFromDatabase(const std::string &name);

  bool isPlaylistFile(const std::string &path) const;
  bool doScanDirectory(
      const std::string &playlistDir,
      std::function<void(int total, int processed)> progressCallback,
      std::stop_token stopToken);

  std::shared_ptr<PlaylistDatabase> db;
  std::shared_ptr<MusicRepository> musicRepo;

  std::chrono::seconds defaultTtl{60};
  mutable std::shared_mutex mutex;
  mutable CachedPlaylistList playlistListCache;
  mutable std::unordered_map<std::string, CachedPlaylist> playlistCache;

  std::jthread scanThread;
  std::stop_source stopSource;
  std::promise<bool> scanPromise;
  std::shared_future<bool> scanFuture;
  mutable std::mutex scanMutex;
};
