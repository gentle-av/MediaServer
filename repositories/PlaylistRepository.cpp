#include "PlaylistRepository.h"
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

PlaylistRepository::PlaylistRepository(
    std::unique_ptr<PlaylistDatabase> db,
    std::unique_ptr<MusicRepository> musicRepo)
    : db(std::move(db)), musicRepo(std::move(musicRepo)) {}

bool PlaylistRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTTL;
}

std::vector<std::string> PlaylistRepository::getPlaylistNames() const {
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
  auto playlists = loadAllPlaylistNamesFromDB();
  std::unique_lock lock(mutex);
  playlistListCache.data = playlists;
  playlistListCache.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const Playlist>
PlaylistRepository::getPlaylist(const std::string &name) const {
  std::shared_lock lock(mutex);
  auto it = playlistCache.find(name);
  if (it != playlistCache.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto playlist = loadPlaylistFromDB(name);
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

bool PlaylistRepository::hasPlaylist(const std::string &name) const {
  return db->playlistExists(name);
}

bool PlaylistRepository::savePlaylist(const std::string &name,
                                      const Playlist &playlist) {
  bool success = savePlaylistToDB(name, playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::savePlaylist(const std::string &name,
                                      Playlist &&playlist) {
  bool success = savePlaylistToDB(name, playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::deletePlaylist(const std::string &name) {
  bool success = deletePlaylistFromDB(name);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::renamePlaylist(const std::string &oldName,
                                        const std::string &newName) {
  if (oldName == newName || hasPlaylist(newName)) {
    return false;
  }
  auto playlist = loadPlaylistFromDB(oldName);
  if (!playlist) {
    return false;
  }
  if (!deletePlaylistFromDB(oldName)) {
    return false;
  }
  bool success = savePlaylistToDB(newName, *playlist);
  if (success) {
    invalidateAll();
  }
  return success;
}

bool PlaylistRepository::createPlaylistFromArtist(
    const std::string &name, const std::string &artistName) {
  auto tracks = musicRepo->getTracksByArtist(artistName);
  if (!tracks || tracks->empty()) {
    return false;
  }
  Playlist playlist(*tracks);
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::createPlaylistFromAlbum(
    const std::string &name, const std::string &albumName,
    const std::string &artistName) {
  auto tracks = musicRepo->getTracksByAlbum(albumName, artistName);
  if (!tracks || tracks->empty()) {
    return false;
  }
  Playlist playlist(*tracks);
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::createPlaylistFromSearch(const std::string &name,
                                                  const std::string &query) {
  auto allTracks = musicRepo->getAllTracks();
  if (!allTracks || allTracks->empty()) {
    return false;
  }
  std::vector<MusicMetadata> matchingTracks;
  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                 ::tolower);
  for (const auto &track : *allTracks) {
    std::string titleLower = track.title;
    std::string artistLower = track.artist;
    std::string albumLower = track.album;
    std::string genreLower = track.genre;
    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(),
                   ::tolower);
    std::transform(artistLower.begin(), artistLower.end(), artistLower.begin(),
                   ::tolower);
    std::transform(albumLower.begin(), albumLower.end(), albumLower.begin(),
                   ::tolower);
    std::transform(genreLower.begin(), genreLower.end(), genreLower.begin(),
                   ::tolower);
    if (titleLower.find(queryLower) != std::string::npos ||
        artistLower.find(queryLower) != std::string::npos ||
        albumLower.find(queryLower) != std::string::npos ||
        genreLower.find(queryLower) != std::string::npos) {
      matchingTracks.push_back(track);
    }
  }
  if (matchingTracks.empty()) {
    return false;
  }
  Playlist playlist(std::move(matchingTracks));
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::importPlaylist(const std::string &filePath) {
  fs::path path(filePath);
  std::string name = path.stem().string();
  if (hasPlaylist(name)) {
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
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::exportPlaylist(const std::string &name,
                                        const std::string &filePath) const {
  auto playlist = getPlaylist(name);
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
PlaylistRepository::loadPlaylistFromDB(const std::string &name) const {
  auto result = db->loadPlaylist(name);
  if (!result.has_value()) {
    return nullptr;
  }
  return std::make_shared<const Playlist>(std::move(result.value()));
}

std::vector<std::string>
PlaylistRepository::loadAllPlaylistNamesFromDB() const {
  return db->getAllPlaylistNames();
}

bool PlaylistRepository::savePlaylistToDB(const std::string &name,
                                          const Playlist &playlist) {
  return db->savePlaylist(name, playlist);
}

bool PlaylistRepository::deletePlaylistFromDB(const std::string &name) {
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

std::shared_future<bool> PlaylistRepository::scanPlaylistDirectoryAsync(
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
  scanThread = std::jthread(
      [this, playlistDir, progressCallback](std::stop_token stopToken) {
        bool success =
            doScanPlaylistDirectory(playlistDir, progressCallback, stopToken);
        scanPromise.set_value(success);
      });
  return scanFuture;
}

bool PlaylistRepository::doScanPlaylistDirectory(
    const std::string &playlistDir,
    std::function<void(int total, int processed)> progressCallback,
    std::stop_token stopToken) {
  try {
    if (stopToken.stop_requested())
      return false;
    if (!fs::exists(playlistDir) || !fs::is_directory(playlistDir)) {
      std::cerr << "[PlaylistRepository] Playlist directory does not exist: "
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
    auto existingNames = loadAllPlaylistNamesFromDB();
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
        if (importPlaylist(path)) {
          imported++;
        } else {
          errors++;
          std::cerr << "[PlaylistRepository] Failed to import playlist: "
                    << path << std::endl;
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
              << ", errors " << errors << " files" << std::endl;
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
