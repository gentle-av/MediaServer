#include "MetadataCache.h"

MetadataCache::MetadataCache(size_t maxSize) : maxSize_(maxSize) {}

MusicMetadata *MetadataCache::get(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = cache.find(filePath);
  if (it != cache.end()) {
    it->second.lastAccess = std::chrono::steady_clock::now();
    return &it->second.metadata;
  }
  return nullptr;
}

void MetadataCache::put(const std::string &filePath,
                        const MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(mutex);
  if (cache.size() >= maxSize_)
    cleanup();
  CachedItem item;
  item.metadata = metadata;
  item.lastAccess = std::chrono::steady_clock::now();
  cache[filePath] = item;
}

void MetadataCache::erase(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(mutex);
  cache.erase(filePath);
}

void MetadataCache::clear() {
  std::lock_guard<std::mutex> lock(mutex);
  cache.clear();
}

void MetadataCache::cleanup() {
  auto now = std::chrono::steady_clock::now();
  for (auto it = cache.begin(); it != cache.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.lastAccess)
                   .count();
    if (age > 3600) {
      it = cache.erase(it);
    } else {
      ++it;
    }
  }
  if (cache.size() > maxSize_) {
    size_t toErase = cache.size() - maxSize_;
    auto it = cache.begin();
    for (size_t i = 0; i < toErase && it != cache.end(); ++i) {
      it = cache.erase(it);
    }
  }
}
