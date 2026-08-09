#include "MusicRepository.h"
#include "../services/music/MetadataExtractor.h"
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

MusicRepository::MusicRepository(MusicDatabase &db) : db_(db) {}

bool MusicRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTTL_;
}

std::vector<std::string> MusicRepository::getArtists() const {
  std::shared_lock lock(mutex_);
  if (!artistsCache_.data.empty() && !isExpired(artistsCache_.timestamp))
    return artistsCache_.data;
  lock.unlock();
  refreshArtistsCache();
  std::shared_lock readLock(mutex_);
  return artistsCache_.data;
}

void MusicRepository::refreshArtistsCache() const {
  auto artists = db_.getArtistsRaw();
  std::unique_lock lock(mutex_);
  artistsCache_.data = artists;
  artistsCache_.timestamp = std::chrono::steady_clock::now();
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicRepository::getAlbums(const std::string &artistFilter) const {
  std::shared_lock lock(mutex_);
  if (!albumsCache_.data.empty() && !isExpired(albumsCache_.timestamp) &&
      albumsCache_.filter == artistFilter) {
    return albumsCache_.data;
  }
  lock.unlock();
  refreshAlbumsCache(artistFilter);
  std::shared_lock readLock(mutex_);
  return albumsCache_.data;
}

void MusicRepository::refreshAlbumsCache(
    const std::string &artistFilter) const {
  auto albums = db_.getAlbumsRaw(artistFilter);
  std::unique_lock lock(mutex_);
  albumsCache_.data = albums;
  albumsCache_.filter = artistFilter;
  albumsCache_.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getTracksByArtist(const std::string &artistName) const {
  std::shared_lock lock(mutex_);
  auto it = tracksByArtistCache_.find(artistName);
  if (it != tracksByArtistCache_.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto tracks = loadTracksByArtistFromDB(artistName);
  std::unique_lock writeLock(mutex_);
  CachedTracks cache;
  cache.data = tracks;
  cache.timestamp = std::chrono::steady_clock::now();
  tracksByArtistCache_[artistName] = cache;
  return tracks;
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadTracksByArtistFromDB(const std::string &artistName) const {
  auto tracks = db_.getTracksByArtistRaw(artistName);
  return std::make_shared<const std::vector<MusicMetadata>>(std::move(tracks));
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getTracksByAlbum(const std::string &albumName,
                                  const std::string &artistName) const {
  std::string key = albumName + "|" + artistName;
  std::shared_lock lock(mutex_);
  auto it = tracksByAlbumCache_.find(key);
  if (it != tracksByAlbumCache_.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto tracks = loadTracksByAlbumFromDB(albumName, artistName);
  std::unique_lock writeLock(mutex_);
  CachedTracks cache;
  cache.data = tracks;
  cache.timestamp = std::chrono::steady_clock::now();
  tracksByAlbumCache_[key] = cache;
  return tracks;
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadTracksByAlbumFromDB(const std::string &albumName,
                                         const std::string &artistName) const {
  auto tracks = db_.getTracksByAlbumRaw(albumName, artistName);
  return std::make_shared<const std::vector<MusicMetadata>>(std::move(tracks));
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::getAllTracks() const {
  std::shared_lock lock(mutex_);
  if (allTracksCache_.data && !isExpired(allTracksCache_.timestamp))
    return allTracksCache_.data;
  lock.unlock();
  refreshAllTracksCache();
  std::shared_lock readLock(mutex_);
  return allTracksCache_.data;
}

void MusicRepository::refreshAllTracksCache() const {
  auto tracks = loadAllTracksFromDB();
  std::unique_lock lock(mutex_);
  allTracksCache_.data = tracks;
  allTracksCache_.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const std::vector<MusicMetadata>>
MusicRepository::loadAllTracksFromDB() const {
  auto paths = db_.getAllFilePaths();
  auto tracks = std::make_shared<std::vector<MusicMetadata>>();
  tracks->reserve(paths.size());
  for (const auto &path : paths) {
    MusicMetadata meta;
    if (db_.getMetadata(path, meta))
      tracks->push_back(meta);
  }
  return tracks;
}

std::optional<MusicMetadata>
MusicRepository::getTrack(const std::string &filePath) const {
  std::shared_lock lock(mutex_);
  auto it = tracksByArtistCache_.find(filePath);
  if (it != tracksByArtistCache_.end() && it->second.data) {
    for (const auto &track : *it->second.data) {
      if (track.filePath == filePath)
        return track;
    }
  }
  lock.unlock();
  MusicMetadata meta;
  if (db_.getMetadata(filePath, meta))
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
  bool success = db_.addFile(filePath, metadata);
  if (success)
    invalidateAll();
  return success;
}

bool MusicRepository::removeTrack(const std::string &filePath) {
  bool success = db_.removeFile(filePath);
  if (success)
    invalidateAll();
  return success;
}

bool MusicRepository::updateTrack(const std::string &filePath,
                                  const MusicMetadata &metadata) {
  bool success = db_.removeFile(filePath) && db_.addFile(filePath, metadata);
  if (success)
    invalidateAll();
  return success;
}

void MusicRepository::invalidateAll() {
  std::unique_lock lock(mutex_);
  invalidateAllLocked();
}

void MusicRepository::invalidateAllLocked() {
  artistsCache_.data.clear();
  albumsCache_.data.clear();
  tracksByArtistCache_.clear();
  tracksByAlbumCache_.clear();
  allTracksCache_.data.reset();
}

size_t MusicRepository::getCacheSize() const {
  std::shared_lock lock(mutex_);
  size_t size = tracksByArtistCache_.size() + tracksByAlbumCache_.size();
  if (!artistsCache_.data.empty())
    size++;
  if (!albumsCache_.data.empty())
    size++;
  if (allTracksCache_.data)
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
  std::lock_guard<std::mutex> lock(scanMutex_);
  if (scanFuture_.valid()) {
    auto status = scanFuture_.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::timeout) {
      return std::shared_future<bool>();
    }
  }
  stopSource_ = std::stop_source{};
  scanPromise_ = std::promise<bool>();
  scanFuture_ = scanPromise_.get_future().share();
  scanThread_ = std::jthread([this, musicDir,
                              progressCallback](std::stop_token stopToken) {
    bool success = doScanMusicDirectory(musicDir, progressCallback, stopToken);
    scanPromise_.set_value(success);
  });
  return scanFuture_;
}

bool MusicRepository::doScanMusicDirectory(
    const std::string &musicDir,
    std::function<void(int total, int processed)> progressCallback,
    std::stop_token stopToken) {
  try {
    if (stopToken.stop_requested())
      return false;
    if (!fs::exists(musicDir) || !fs::is_directory(musicDir)) {
      std::cerr << "[MusicRepository] Music directory does not exist: "
                << musicDir << std::endl;
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
    std::cout << "[MusicRepository] Found " << allFiles.size() << " music files"
              << std::endl;
    auto existingPaths = db_.getAllFilePaths();
    std::unordered_set<std::string> existingSet(existingPaths.begin(),
                                                existingPaths.end());
    std::unordered_set<std::string> foundSet;
    int total = static_cast<int>(allFiles.size());
    int processed = 0;
    int added = 0;
    int errors = 0;
    for (const auto &path : allFiles) {
      if (stopToken.stop_requested())
        return false;
      foundSet.insert(path);
      if (existingSet.find(path) == existingSet.end()) {
        MusicMetadata metadata;
        if (MetadataExtractor::extractMetadata(path, metadata)) {
          metadata.filePath = path;
          if (db_.addFile(path, metadata)) {
            added++;
            std::vector<char> albumArt;
            if (MetadataExtractor::extractAlbumArt(path, albumArt)) {
              db_.saveAlbumArt(path, albumArt);
            }
          }
        } else {
          errors++;
          std::cerr << "[MusicRepository] Failed to extract metadata: " << path
                    << std::endl;
        }
      }
      processed++;
      if (progressCallback)
        progressCallback(total, processed);
    }
    int removed = 0;
    for (const auto &path : existingSet) {
      if (stopToken.stop_requested())
        return false;
      if (foundSet.find(path) == foundSet.end()) {
        if (db_.removeFile(path))
          removed++;
      }
    }
    {
      std::unique_lock lock(mutex_);
      invalidateAllLocked();
    }
    std::cout << "[MusicRepository] Scan completed: added " << added
              << ", removed " << removed << ", errors " << errors << " files"
              << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[MusicRepository] Scan error: " << e.what() << std::endl;
    return false;
  }
}

void MusicRepository::cancelScan() { stopSource_.request_stop(); }

bool MusicRepository::isScanning() const {
  std::lock_guard<std::mutex> lock(scanMutex_);
  return scanFuture_.valid() &&
         scanFuture_.wait_for(std::chrono::milliseconds(0)) ==
             std::future_status::timeout;
}

void MusicRepository::waitForScan() {
  std::lock_guard<std::mutex> lock(scanMutex_);
  if (scanFuture_.valid()) {
    scanFuture_.wait();
  }
}
