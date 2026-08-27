#pragma once

#include "../database/MusicDatabase.h"
#include "../events/EventBus.h"
#include "../models/MusicMetadata.h"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class MusicRepository {
public:
  explicit MusicRepository(std::shared_ptr<MusicDatabase> db);
  ~MusicRepository() = default;

  std::vector<std::string> getArtists() const;
  std::vector<std::tuple<std::string, std::string, std::string>>
  getAlbums(const std::string &artistFilter = "") const;
  std::shared_ptr<const std::vector<MusicMetadata>>
  getTracksByArtist(const std::string &artistName) const;
  std::shared_ptr<const std::vector<MusicMetadata>>
  getTracksByAlbum(const std::string &albumName,
                   const std::string &artistName = "") const;
  std::shared_ptr<const std::vector<MusicMetadata>> getAllTracks() const;
  std::optional<MusicMetadata> getTrack(const std::string &filePath) const;
  void forEachTrack(std::function<void(const MusicMetadata &)> callback) const;
  void forEachArtist(std::function<void(const std::string &)> callback) const;

  bool addTrack(const std::string &filePath, const MusicMetadata &metadata);
  bool removeTrack(const std::string &filePath);
  bool updateTrack(const std::string &filePath, const MusicMetadata &metadata);

  void invalidateAll();
  void setTTL(std::chrono::seconds ttl) { defaultTTL = ttl; }
  size_t getCacheSize() const;

  std::shared_future<bool> scanMusicDirectoryAsync(
      const std::string &musicDir,
      std::function<void(int total, int processed)> progressCallback = nullptr);
  void cancelScan();
  bool isScanning() const;
  void waitForScan();

  EventBus &getEventBus() { return eventBus; }
  void subscribe(const std::string &event, EventBus::EventCallback callback) {
    eventBus.subscribe(event, callback);
  }

private:
  struct CachedTracks {
    std::shared_ptr<const std::vector<MusicMetadata>> data;
    std::chrono::steady_clock::time_point timestamp;
  };
  struct CachedArtists {
    std::vector<std::string> data;
    std::chrono::steady_clock::time_point timestamp;
  };
  struct CachedAlbums {
    std::vector<std::tuple<std::string, std::string, std::string>> data;
    std::string filter;
    std::chrono::steady_clock::time_point timestamp;
  };

  bool isExpired(const std::chrono::steady_clock::time_point &timestamp) const;
  void refreshArtistsCache() const;
  void refreshAlbumsCache(const std::string &artistFilter) const;
  void refreshAllTracksCache() const;
  void invalidateAllLocked();

  std::shared_ptr<const std::vector<MusicMetadata>>
  loadTracksByArtistFromDB(const std::string &artistName) const;
  std::shared_ptr<const std::vector<MusicMetadata>>
  loadTracksByAlbumFromDB(const std::string &albumName,
                          const std::string &artistName) const;
  std::shared_ptr<const std::vector<MusicMetadata>> loadAllTracksFromDB() const;

  bool isMusicFile(const std::string &path) const;
  bool doScanMusicDirectory(
      const std::string &musicDir,
      std::function<void(int total, int processed)> progressCallback,
      std::stop_token stopToken);

  std::shared_ptr<MusicDatabase> db;
  std::chrono::seconds defaultTTL{60};
  mutable std::shared_mutex mutex;
  mutable CachedArtists artistsCache;
  mutable CachedAlbums albumsCache;
  mutable std::unordered_map<std::string, CachedTracks> tracksByArtistCache;
  mutable std::unordered_map<std::string, CachedTracks> tracksByAlbumCache;
  mutable CachedTracks allTracksCache;

  std::jthread scanThread;
  std::stop_source stopSource;
  std::promise<bool> scanPromise;
  std::shared_future<bool> scanFuture;
  mutable std::mutex scanMutex;

  EventBus eventBus;
};
