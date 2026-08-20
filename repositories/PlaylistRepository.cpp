// PlaylistRepository.cpp - добавление конструктора с подпиской
#include "PlaylistRepository.h"
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

PlaylistRepository::PlaylistRepository(
    std::unique_ptr<PlaylistDatabase> db,
    std::unique_ptr<MusicRepository> musicRepo)
    : db(std::move(db)), musicRepo(std::move(musicRepo)) {
  this->musicRepo->subscribe(
      "track_removed",
      [this](const std::string &filePath) { onTrackRemoved(filePath); });
}

bool PlaylistRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTTL;
}

std::vector<std::string> PlaylistRepository::getAllPlaylistNames() const {
  std::shared_lock lock(mutex);
  if (!playlistListCache.data.empty() &&
      !isExpired(playlistListCache.timestamp))
    return playlistListCache.data;
  lock.unlock();
  refreshPlaylistListCache();
  std::shared_lock readLock(mutex);
  return playlistListCache.data;
}

void PlaylistRepository::refreshPlaylistListCache() const {
  auto playlists = db->getAllPlaylistNames();
  std::unique_lock lock(mutex);
  playlistListCache.data = playlists;
  playlistListCache.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const Playlist>
PlaylistRepository::loadPlaylist(const std::string &name) const {
  std::shared_lock lock(mutex);
  auto it = playlistCache.find(name);
  if (it != playlistCache.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto playlist = loadFromDatabase(name);
  if (!playlist) {
    return nullptr;
  }
  std::unique_lock writeLock(mutex);
  CachedPlaylist cache;
  cache.data = playlist;
  cache.timestamp = std::chrono::steady_clock::now();
  playlistCache[name] = cache;
  return playlist;
}

int PlaylistRepository::getTrackCount(const std::string &name) const {
  return db->getTrackCount(name);
}

bool PlaylistRepository::playlistExists(const std::string &name) const {
  return db->playlistExists(name);
}

bool PlaylistRepository::savePlaylist(const std::string &name,
                                      const Playlist &playlist) {
  bool success = saveToDatabase(name, playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::savePlaylist(const std::string &name,
                                      Playlist &&playlist) {
  bool success = saveToDatabase(name, playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::deletePlaylist(const std::string &name) {
  bool success = deleteFromDatabase(name);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::renamePlaylist(const std::string &oldName,
                                        const std::string &newName) {
  if (oldName == newName || playlistExists(newName)) {
    return false;
  }
  auto playlist = loadFromDatabase(oldName);
  if (!playlist) {
    return false;
  }
  if (!deleteFromDatabase(oldName)) {
    return false;
  }
  bool success = saveToDatabase(newName, *playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

Playlist
PlaylistRepository::createFromArtist(const std::string &artistName) const {
  auto tracks = musicRepo->getTracksByArtist(artistName);
  if (!tracks || tracks->empty()) {
    return Playlist(std::vector<MusicMetadata>{});
  }
  return Playlist(*tracks);
}

Playlist
PlaylistRepository::createFromAlbum(const std::string &albumName,
                                    const std::string &artistName) const {
  auto tracks = musicRepo->getTracksByAlbum(albumName, artistName);
  if (!tracks || tracks->empty()) {
    return Playlist(std::vector<MusicMetadata>{});
  }
  return Playlist(*tracks);
}

Playlist PlaylistRepository::createFromSearch(const std::string &query) const {
  auto allTracks = musicRepo->getAllTracks();
  if (!allTracks || allTracks->empty()) {
    return Playlist(std::vector<MusicMetadata>{});
  }
  std::vector<MusicMetadata> matchingTracks;
  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                 ::tolower);
  for (const auto &track : *allTracks) {
    std::string titleLower = track.title;
    std::string artistLower = track.artist;
    std::string albumLower = track.album;
    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(),
                   ::tolower);
    std::transform(artistLower.begin(), artistLower.end(), artistLower.begin(),
                   ::tolower);
    std::transform(albumLower.begin(), albumLower.end(), albumLower.begin(),
                   ::tolower);
    if (titleLower.find(queryLower) != std::string::npos ||
        artistLower.find(queryLower) != std::string::npos ||
        albumLower.find(queryLower) != std::string::npos) {
      matchingTracks.push_back(track);
    }
  }
  return Playlist(std::move(matchingTracks));
}

Playlist PlaylistRepository::createFromFilePaths(
    const std::vector<std::string> &paths) const {
  std::vector<MusicMetadata> tracks;
  tracks.reserve(paths.size());
  for (const auto &path : paths) {
    auto track = musicRepo->getTrack(path);
    if (track.has_value()) {
      tracks.push_back(track.value());
    } else {
      MusicMetadata meta;
      meta.filePath = path;
      tracks.push_back(meta);
    }
  }
  return Playlist(std::move(tracks));
}

bool PlaylistRepository::createAndSaveFromArtist(
    const std::string &name, const std::string &artistName) {
  auto playlist = createFromArtist(artistName);
  if (playlist.empty()) {
    return false;
  }
  return savePlaylist(name, std::move(playlist));
}

bool PlaylistRepository::createAndSaveFromAlbum(const std::string &name,
                                                const std::string &albumName,
                                                const std::string &artistName) {
  auto playlist = createFromAlbum(albumName, artistName);
  if (playlist.empty()) {
    return false;
  }
  return savePlaylist(name, std::move(playlist));
}

bool PlaylistRepository::createAndSaveFromSearch(const std::string &name,
                                                 const std::string &query) {
  auto playlist = createFromSearch(query);
  if (playlist.empty()) {
    return false;
  }
  return savePlaylist(name, std::move(playlist));
}

bool PlaylistRepository::importFromFile(const std::string &filePath) {
  fs::path path(filePath);
  std::string name = path.stem().string();
  if (playlistExists(name)) {
    std::cerr << "[PlaylistRepository] Playlist already exists: " << name
              << std::endl;
    return false;
  }
  Playlist playlist(std::vector<MusicMetadata>{});
  if (!playlist.load(filePath)) {
    std::cerr << "[PlaylistRepository] Failed to load playlist: " << filePath
              << std::endl;
    return false;
  }
  return savePlaylist(name, std::move(playlist));
}

bool PlaylistRepository::exportToFile(const std::string &name,
                                      const std::string &filePath) const {
  auto playlist = loadPlaylist(name);
  if (!playlist) {
    return false;
  }
  return playlist->save(filePath);
}

void PlaylistRepository::invalidateAll() {
  std::unique_lock lock(mutex);
  invalidateAllLocked();
}

void PlaylistRepository::invalidateAllLocked() {
  playlistListCache.data.clear();
  playlistCache.clear();
}

size_t PlaylistRepository::getCacheSize() const {
  std::shared_lock lock(mutex);
  size_t size = playlistCache.size();
  if (!playlistListCache.data.empty())
    size++;
  return size;
}

std::shared_ptr<const Playlist>
PlaylistRepository::loadFromDatabase(const std::string &name) const {
  auto result = db->loadPlaylist(name);
  if (!result.has_value()) {
    return nullptr;
  }
  return std::make_shared<const Playlist>(std::move(result.value()));
}

bool PlaylistRepository::saveToDatabase(const std::string &name,
                                        const Playlist &playlist) {
  return db->savePlaylist(name, playlist);
}

bool PlaylistRepository::deleteFromDatabase(const std::string &name) {
  return db->deletePlaylist(name);
}

bool PlaylistRepository::isPlaylistFile(const std::string &path) const {
  static const std::unordered_set<std::string> extensions = {".json", ".m3u",
                                                             ".m3u8", ".pls"};
  fs::path filePath(path);
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return extensions.find(ext) != extensions.end();
}

std::shared_future<bool> PlaylistRepository::scanDirectoryAsync(
    const std::string &playlistDir,
    std::function<void(int total, int processed)> progressCallback) {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid()) {
    auto status = scanFuture.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::timeout) {
      return std::shared_future<bool>();
    }
  }
  stopSource = std::stop_source{};
  scanPromise = std::promise<bool>();
  scanFuture = scanPromise.get_future().share();
  scanThread = std::jthread([this, playlistDir,
                             progressCallback](std::stop_token stopToken) {
    bool success = doScanDirectory(playlistDir, progressCallback, stopToken);
    scanPromise.set_value(success);
  });
  return scanFuture;
}

bool PlaylistRepository::doScanDirectory(
    const std::string &playlistDir,
    std::function<void(int total, int processed)> progressCallback,
    std::stop_token stopToken) {
  try {
    if (stopToken.stop_requested())
      return false;
    if (!fs::exists(playlistDir) || !fs::is_directory(playlistDir)) {
      std::cerr << "[PlaylistRepository] Directory does not exist: "
                << playlistDir << std::endl;
      return false;
    }
    std::vector<std::string> allFiles;
    for (const auto &entry : fs::recursive_directory_iterator(playlistDir)) {
      if (stopToken.stop_requested())
        return false;
      if (entry.is_regular_file() && isPlaylistFile(entry.path().string())) {
        allFiles.push_back(entry.path().string());
      }
    }
    std::cout << "[PlaylistRepository] Found " << allFiles.size()
              << " playlist files" << std::endl;
    auto existingNames = db->getAllPlaylistNames();
    std::unordered_set<std::string> existingSet(existingNames.begin(),
                                                existingNames.end());
    int total = static_cast<int>(allFiles.size());
    int processed = 0;
    int imported = 0;
    int errors = 0;
    for (const auto &path : allFiles) {
      if (stopToken.stop_requested())
        return false;
      fs::path filePath(path);
      std::string name = filePath.stem().string();
      if (existingSet.find(name) == existingSet.end()) {
        if (importFromFile(path)) {
          imported++;
        } else {
          errors++;
          std::cerr << "[PlaylistRepository] Failed to import: " << path
                    << std::endl;
        }
      }
      processed++;
      if (progressCallback)
        progressCallback(total, processed);
    }
    {
      std::unique_lock lock(mutex);
      invalidateAllLocked();
    }
    std::cout << "[PlaylistRepository] Scan completed: imported " << imported
              << ", errors " << errors << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[PlaylistRepository] Scan error: " << e.what() << std::endl;
    return false;
  }
}

void PlaylistRepository::cancelScan() { stopSource.request_stop(); }

bool PlaylistRepository::isScanning() const {
  std::lock_guard<std::mutex> lock(scanMutex);
  return scanFuture.valid() &&
         scanFuture.wait_for(std::chrono::milliseconds(0)) ==
             std::future_status::timeout;
}

void PlaylistRepository::waitForScan() {
  std::lock_guard<std::mutex> lock(scanMutex);
  if (scanFuture.valid()) {
    scanFuture.wait();
  }
}

void PlaylistRepository::validateAllPlaylists() {
  auto playlistNames = getAllPlaylistNames();
  for (const auto &name : playlistNames) {
    validatePlaylist(name);
  }
}

void PlaylistRepository::validatePlaylist(const std::string &name) {
  auto playlist = loadPlaylist(name);
  if (!playlist) {
    return;
  }
  auto tracks = playlist->getAllTracks();
  std::vector<std::string> pathsToRemove;
  for (const auto &track : tracks) {
    auto existingTrack = musicRepo->getTrack(track.filePath);
    if (!existingTrack.has_value()) {
      pathsToRemove.push_back(track.filePath);
    }
  }
  if (!pathsToRemove.empty()) {
    Playlist cleanedPlaylist(*playlist);
    for (const auto &path : pathsToRemove) {
      cleanedPlaylist.removeByFilePath(path);
    }
    savePlaylist(name, cleanedPlaylist);
    std::cout << "[PlaylistRepository] Cleaned playlist '" << name
              << "': removed " << pathsToRemove.size() << " missing tracks"
              << std::endl;
  }
}

void PlaylistRepository::onTrackRemoved(const std::string &filePath) {
  std::unique_lock lock(mutex);
  for (auto &[name, cached] : playlistCache) {
    if (cached.data) {
      Playlist updatedPlaylist(*cached.data);
      if (updatedPlaylist.removeByFilePath(filePath)) {
        cached.data = std::make_shared<Playlist>(updatedPlaylist);
        cached.timestamp = std::chrono::steady_clock::now();
        saveToDatabase(name, updatedPlaylist);
        std::cout << "[PlaylistRepository] Updated playlist '" << name
                  << "' after track removal" << std::endl;
      }
    }
  }
}
