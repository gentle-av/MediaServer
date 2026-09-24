#include "MusicRepository.h"
#include "../services/music/MetadataExtractor.h"
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

MusicRepository::MusicRepository(std::shared_ptr<MusicDatabase> db)
    : db(std::move(db)) {}

bool MusicRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTtl;
}

std::vector<std::string> MusicRepository::getArtists() const {
  std::shared_lock readLock(mutex);
  if (!artistsCache.data.empty() && !isExpired(artistsCache.timestamp)) {
    return artistsCache.data;
  }
  readLock.unlock();
  auto freshlyLoadedArtists = db->getArtistsRaw();
  std::unique_lock writeLock(mutex);
  artistsCache.data = freshlyLoadedArtists;
  artistsCache.timestamp = std::chrono::steady_clock::now();
  return artistsCache.data;
}

void MusicRepository::refreshArtistsCache() const {
  auto artists = db->getArtistsRaw();
  std::unique_lock lock(mutex);
  artistsCache.data = artists;
  artistsCache.timestamp = std::chrono::steady_clock::now();
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicRepository::getAlbums(const std::string &artistFilter) const {
  std::shared_lock readLock(mutex);
  if (albumsCache.data.size() > 0 && !isExpired(albumsCache.timestamp) &&
      albumsCache.filter == artistFilter) {
    return albumsCache.data;
  }
  readLock.unlock();
  auto freshlyLoadedAlbums = db->getAlbumsRaw(artistFilter);
  std::unique_lock writeLock(mutex);
  albumsCache.data = freshlyLoadedAlbums;
  albumsCache.filter = artistFilter;
  albumsCache.timestamp = std::chrono::steady_clock::now();
  return albumsCache.data;
}

void MusicRepository::refreshAlbumsCache(
    const std::string &artistFilter) const {
  auto albums = db->getAlbumsRaw(artistFilter);
  std::unique_lock lock(mutex);
  albumsCache.data = albums;
  albumsCache.filter = artistFilter;
  albumsCache.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getTracksByArtist(const std::string &artistName) const {
  std::shared_lock lock(mutex);
  auto it = tracksByArtistCache.find(artistName);
  if (it != tracksByArtistCache.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto tracks = loadTracksByArtistFromDB(artistName);
  std::unique_lock writeLock(mutex);
  CachedTracks cache;
  cache.data = tracks;
  cache.timestamp = std::chrono::steady_clock::now();
  tracksByArtistCache[artistName] = cache;
  return tracks;
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadTracksByArtistFromDB(const std::string &artistName) const {
  auto tracks = db->getTracksByArtistRaw(artistName);
  return std::make_shared<const std::vector<MusicMetadata>>(std::move(tracks));
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getTracksByAlbum(const std::string &albumName,
                                  const std::string &artistName) const {
  std::string key = albumName + "|" + artistName;
  std::shared_lock lock(mutex);
  auto it = tracksByAlbumCache.find(key);
  if (it != tracksByAlbumCache.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto tracks = loadTracksByAlbumFromDB(albumName, artistName);
  std::unique_lock writeLock(mutex);
  CachedTracks cache;
  cache.data = tracks;
  cache.timestamp = std::chrono::steady_clock::now();
  tracksByAlbumCache[key] = cache;
  return tracks;
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadTracksByAlbumFromDB(const std::string &albumName,
                                         const std::string &artistName) const {
  auto tracks = db->getTracksByAlbumRaw(albumName, artistName);
  return std::make_shared<const std::vector<MusicMetadata>>(std::move(tracks));
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getAllTracks() const {
  std::shared_lock lock(mutex);
  if (allTracksCache.data && !isExpired(allTracksCache.timestamp))
    return allTracksCache.data;
  lock.unlock();
  refreshAllTracksCache();
  std::shared_lock readLock(mutex);
  return allTracksCache.data;
}

void MusicRepository::refreshAllTracksCache() const {
  auto tracks = loadAllTracksFromDB();
  std::unique_lock lock(mutex);
  allTracksCache.data = tracks;
  allTracksCache.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadAllTracksFromDB() const {
  auto paths = db->getAllFilePaths();
  auto tracks = std::make_shared<std::vector<MusicMetadata>>();
  tracks->reserve(paths.size());
  for (const auto &path : paths) {
    if (!fs::exists(path)) {
      continue;
    }
    MusicMetadata meta;
    if (db->getMetadata(path, meta)) {
      tracks->push_back(meta);
    }
  }
  return tracks;
}

std::optional<MusicMetadata>
MusicRepository::getTrack(const std::string &filePath) const {
  std::shared_lock lock(mutex);
  auto it = tracksByArtistCache.find(filePath);
  if (it != tracksByArtistCache.end() && it->second.data) {
    for (const auto &track : *it->second.data) {
      if (track.filePath == filePath)
        return track;
    }
  }
  lock.unlock();
  MusicMetadata meta;
  if (db->getMetadata(filePath, meta))
    return meta;
  return std::nullopt;
}

void MusicRepository::forEachTrack(
    std::function<void(const MusicMetadata &)> callback) const {
  auto tracks = getAllTracks();
  for (const auto &track : *tracks)
    callback(track);
}

void MusicRepository::forEachArtist(
    std::function<void(const std::string &)> callback) const {
  auto artists = getArtists();
  for (const auto &artist : artists)
    callback(artist);
}

bool MusicRepository::addTrack(const std::string &filePath,
                               const MusicMetadata &metadata) {
  bool success = db->addFile(filePath, metadata);
  if (success)
    invalidateAll();
  return success;
}

bool MusicRepository::removeTrack(const std::string &filePath) {
  bool success = db->removeFile(filePath);
  if (success) {
    std::unique_lock lock(mutex);
    invalidateAllLocked();
    lock.unlock();
    eventBus.publish("track_removed", filePath);
    eventBus.publish("cacheinvalidated");
  }
  return success;
}

bool MusicRepository::updateTrack(const std::string &filePath,
                                  const MusicMetadata &metadata) {
  bool success = db->removeFile(filePath) && db->addFile(filePath, metadata);
  if (success)
    invalidateAll();
  return success;
}

void MusicRepository::invalidateAll() {
  std::unique_lock lock(mutex);
  invalidateAllLocked();
  eventBus.publish("cacheinvalidated");
}

void MusicRepository::invalidateAllLocked() {
  artistsCache.data.clear();
  artistsCache.timestamp = std::chrono::steady_clock::time_point{};
  albumsCache.data.clear();
  albumsCache.filter.clear();
  albumsCache.timestamp = std::chrono::steady_clock::time_point{};
  tracksByArtistCache.clear();
  tracksByAlbumCache.clear();
  allTracksCache.data.reset();
  allTracksCache.timestamp = std::chrono::steady_clock::time_point{};
}

size_t MusicRepository::getCacheSize() const {
  std::shared_lock lock(mutex);
  size_t size = tracksByArtistCache.size() + tracksByAlbumCache.size();
  if (!artistsCache.data.empty())
    size++;
  if (!albumsCache.data.empty())
    size++;
  if (allTracksCache.data)
    size++;
  return size;
}

bool MusicRepository::isMusicFile(const std::string &path) const {
  static const std::unordered_set<std::string> extensions = {
      ".mp3", ".flac", ".m4a", ".wav", ".ogg", ".opus", ".aac", ".wma"};
  fs::path filePath(path);
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return extensions.find(ext) != extensions.end();
}

std::shared_future<bool> MusicRepository::scanMusicDirectoryAsync(
    const std::string &musicDir,
    std::function<void(int total, int processed)> progressCallback) {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid()) {
    auto status = scanFuture.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::timeout) {
      return std::shared_future<bool>();
    }
  }
  {
    std::unique_lock cacheLock(mutex);
    invalidateAllLocked();
  }
  stopSource = std::stop_source{};
  scanPromise = std::promise<bool>();
  scanFuture = scanPromise.get_future().share();
  scanThread = std::jthread([this, musicDir,
                             progressCallback](std::stop_token stopToken) {
    bool success = doScanMusicDirectory(musicDir, progressCallback, stopToken);
    scanPromise.set_value(success);
  });
  return scanFuture;
}

bool MusicRepository::doScanMusicDirectory(
    const std::string &musicDir,
    std::function<void(int total, int processed)> progressCallback,
    std::stop_token stopToken) {
  try {
    if (stopToken.stop_requested())
      return false;
    if (!fs::exists(musicDir) || !fs::is_directory(musicDir)) {
      return false;
    }
    std::vector<std::string> allFiles;
    for (const auto &entry : fs::recursive_directory_iterator(musicDir)) {
      if (stopToken.stop_requested())
        return false;
      if (entry.is_regular_file() && isMusicFile(entry.path().string())) {
        allFiles.push_back(entry.path().string());
      }
    }
    auto existingPaths = db->getAllFilePaths();
    std::unordered_set<std::string> existingSet(existingPaths.begin(),
                                                existingPaths.end());
    std::unordered_set<std::string> foundSet;
    int total = static_cast<int>(allFiles.size());
    int processed = 0;
    for (const auto &path : allFiles) {
      if (stopToken.stop_requested())
        return false;
      foundSet.insert(path);
      if (existingSet.find(path) == existingSet.end()) {
        MusicMetadata metadata;
        if (MetadataExtractor::extractMetadata(path, metadata)) {
          metadata.filePath = path;
          if (db->addFile(path, metadata)) {
            std::vector<char> albumArt;
            if (MetadataExtractor::extractAlbumArt(path, albumArt)) {
              db->saveAlbumArt(path, albumArt);
            }
          }
        }
      }
      processed++;
      if (progressCallback)
        progressCallback(total, processed);
    }
    std::vector<std::string> removedPaths;
    for (const auto &path : existingSet) {
      if (stopToken.stop_requested())
        return false;
      if (foundSet.find(path) == foundSet.end()) {
        if (db->removeFile(path)) {
          removedPaths.push_back(path);
        }
      }
    }
    {
      std::unique_lock lock(mutex);
      invalidateAllLocked();
    }
    for (const auto &path : removedPaths) {
      eventBus.publish("track_removed", path);
    }
    eventBus.publish("scan_completed");
    return true;
  } catch (...) {
    return false;
  }
}

void MusicRepository::cancelScan() { stopSource.request_stop(); }

bool MusicRepository::isScanning() const {
  std::lock_guard<std::mutex> lock(scanMutex);
  return scanFuture.valid() &&
         scanFuture.wait_for(std::chrono::milliseconds(0)) ==
             std::future_status::timeout;
}

void MusicRepository::waitForScan() {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid()) {
    scanFuture.wait();
  }
}

std::shared_ptr<MusicDatabase> MusicRepository::getDatabase() const {
  return db;
}

void MusicRepository::waitForPendingReload() {
  {
    std::unique_lock lock(mutex);
    invalidateAllLocked();
  }
  refreshAllTracksCache();
  refreshArtistsCache();
  refreshAlbumsCache("");
}
