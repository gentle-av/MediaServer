#pragma once

#include "models/MusicMetadata.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

class MetadataCache {
public:
  MetadataCache(size_t maxSize = 500);

  MusicMetadata *get(const std::string &filePath);
  void put(const std::string &filePath, const MusicMetadata &metadata);
  void erase(const std::string &filePath);
  void clear();

private:
  struct CachedItem {
    MusicMetadata metadata;
    std::chrono::steady_clock::time_point lastAccess;
  };

  void cleanup();

  std::unordered_map<std::string, CachedItem> cache_;
  std::mutex mutex_;
  size_t maxSize_;
};
