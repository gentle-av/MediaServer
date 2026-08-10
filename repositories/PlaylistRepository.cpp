#include "PlaylistRepository.h"
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

PlaylistRepository::PlaylistRepository(MusicDatabase &db) : db_(db) {}

bool PlaylistRepository::isExpired(
    const std::chrono::steady_clock::time_point &timestamp) const {
  return std::chrono::steady_clock::now() - timestamp > defaultTTL_;
}

std::vector<std::string> PlaylistRepository::getPlaylistNames() const {
  std::shared_lock lock(mutex_);
  if (!playlistListCache_.data.empty() &&
      !isExpired(playlistListCache_.timestamp))
    return playlistListCache_.data;
  lock.unlock();
  refreshPlaylistListCache();
  std::shared_lock readLock(mutex_);
  return playlistListCache_.data;
}

void PlaylistRepository::refreshPlaylistListCache() const {
  auto playlists = loadAllPlaylistNamesFromDB();
  std::unique_lock lock(mutex_);
  playlistListCache_.data = playlists;
  playlistListCache_.timestamp = std::chrono::steady_clock::now();
}

std::shared_ptr<const Playlist>
PlaylistRepository::getPlaylist(const std::string &name) const {
  std::shared_lock lock(mutex_);
  auto it = playlistCache_.find(name);
  if (it != playlistCache_.end() && !isExpired(it->second.timestamp) &&
      it->second.data) {
    return it->second.data;
  }
  lock.unlock();
  auto playlist = loadPlaylistFromDB(name);
  if (!playlist) {
    return nullptr;
  }
  std::unique_lock writeLock(mutex_);
  CachedPlaylist cache;
  cache.data = playlist;
  cache.timestamp = std::chrono::steady_clock::now();
  playlistCache_[name] = cache;
  return playlist;
}

bool PlaylistRepository::hasPlaylist(const std::string &name) const {
  auto playlists = getPlaylistNames();
  return std::find(playlists.begin(), playlists.end(), name) != playlists.end();
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
  auto tracks = db_.getTracksByArtistRaw(artistName);
  if (tracks.empty()) {
    return false;
  }
  Playlist playlist(std::move(tracks));
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::createPlaylistFromAlbum(
    const std::string &name, const std::string &albumName,
    const std::string &artistName) {
  auto tracks = db_.getTracksByAlbumRaw(albumName, artistName);
  if (tracks.empty()) {
    return false;
  }
  Playlist playlist(std::move(tracks));
  return savePlaylist(name, playlist);
}

bool PlaylistRepository::createPlaylistFromSearch(const std::string &name,
                                                  const std::string &query) {
  auto allTracks = db_.getAllFilePaths();
  std::vector<MusicMetadata> matchingTracks;
  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                 ::tolower);
  for (const auto &path : allTracks) {
    MusicMetadata meta;
    if (db_.getMetadata(path, meta)) {
      std::string titleLower = meta.title;
      std::string artistLower = meta.artist;
      std::string albumLower = meta.album;
      std::string genreLower = meta.genre;
      std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(),
                     ::tolower);
      std::transform(artistLower.begin(), artistLower.end(),
                     artistLower.begin(), ::tolower);
      std::transform(albumLower.begin(), albumLower.end(), albumLower.begin(),
                     ::tolower);
      std::transform(genreLower.begin(), genreLower.end(), genreLower.begin(),
                     ::tolower);
      if (titleLower.find(queryLower) != std::string::npos ||
          artistLower.find(queryLower) != std::string::npos ||
          albumLower.find(queryLower) != std::string::npos ||
          genreLower.find(queryLower) != std::string::npos) {
        matchingTracks.push_back(meta);
      }
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
  std::unique_lock lock(mutex_);
  invalidateAllLocked();
}

void PlaylistRepository::invalidateAllLocked() {
  playlistListCache_.data.clear();
  playlistCache_.clear();
}

size_t PlaylistRepository::getCacheSize() const {
  std::shared_lock lock(mutex_);
  size_t size = playlistCache_.size();
  if (!playlistListCache_.data.empty())
    size++;
  return size;
}

std::shared_ptr<const Playlist>
PlaylistRepository::loadPlaylistFromDB(const std::string &name) const {
  fs::path playlistPath = fs::path("playlists") / (name + ".json");
  if (!fs::exists(playlistPath)) {
    return nullptr;
  }
  auto playlist = std::make_shared<Playlist>(std::vector<MusicMetadata>{});
  if (!playlist->load(playlistPath.string())) {
    return nullptr;
  }
  return playlist;
}

std::vector<std::string>
PlaylistRepository::loadAllPlaylistNamesFromDB() const {
  std::vector<std::string> names;
  fs::path playlistsDir("playlists");
  if (!fs::exists(playlistsDir) || !fs::is_directory(playlistsDir)) {
    return names;
  }
  for (const auto &entry : fs::directory_iterator(playlistsDir)) {
    if (entry.is_regular_file() && isPlaylistFile(entry.path().string())) {
      names.push_back(entry.path().stem().string());
    }
  }
  return names;
}

bool PlaylistRepository::savePlaylistToDB(const std::string &name,
                                          const Playlist &playlist) {
  fs::path playlistsDir("playlists");
  try {
    if (!fs::exists(playlistsDir)) {
      fs::create_directories(playlistsDir);
    }
    fs::path filePath = playlistsDir / (name + ".json");
    return playlist.save(filePath.string());
  } catch (const std::exception &e) {
    std::cerr << "[PlaylistRepository] Failed to save playlist: " << e.what()
              << std::endl;
    return false;
  }
}

bool PlaylistRepository::deletePlaylistFromDB(const std::string &name) {
  fs::path filePath = fs::path("playlists") / (name + ".json");
  try {
    if (!fs::exists(filePath)) {
      return false;
    }
    return fs::remove(filePath);
  } catch (const std::exception &e) {
    std::cerr << "[PlaylistRepository] Failed to delete playlist: " << e.what()
              << std::endl;
    return false;
  }
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
  scanThread_ = std::jthread(
      [this, playlistDir, progressCallback](std::stop_token stopToken) {
        bool success =
            doScanPlaylistDirectory(playlistDir, progressCallback, stopToken);
        scanPromise_.set_value(success);
      });
  return scanFuture_;
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
      std::unique_lock lock(mutex_);
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

void PlaylistRepository::cancelScan() { stopSource_.request_stop(); }

bool PlaylistRepository::isScanning() const {
  std::lock_guard<std::mutex> lock(scanMutex_);
  return scanFuture_.valid() &&
         scanFuture_.wait_for(std::chrono::milliseconds(0)) ==
             std::future_status::timeout;
}

void PlaylistRepository::waitForScan() {
  std::lock_guard<std::mutex> lock(scanMutex_);
  if (scanFuture_.valid()) {
    scanFuture_.wait();
  }
}
