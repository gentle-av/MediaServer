#include "services/music/MetadataCache.h"

MetadataCache::MetadataCache(size_t maxSize) : maxSize_(maxSize) {}

MusicMetadata *MetadataCache::get(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = cache_.find(filePath);
  if (it != cache_.end()) {
    it->second.lastAccess = std::chrono::steady_clock::now();
    return &it->second.metadata;
  }
  return nullptr;
}

void MetadataCache::put(const std::string &filePath,
                        const MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cache_.size() >= maxSize_)
    cleanup();
  CachedItem item;
  item.metadata = metadata;
  item.lastAccess = std::chrono::steady_clock::now();
  cache_[filePath] = item;
}

void MetadataCache::erase(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.erase(filePath);
}

void MetadataCache::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

void MetadataCache::cleanup() {
  auto now = std::chrono::steady_clock::now();
  for (auto it = cache_.begin(); it != cache_.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.lastAccess)
                   .count();
    if (age > 3600) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
  if (cache_.size() > maxSize_) {
    size_t toErase = cache_.size() - maxSize_;
    auto it = cache_.begin();
    for (size_t i = 0; i < toErase && it != cache_.end(); ++i) {
      it = cache_.erase(it);
    }
  }
}
