#pragma once

#include "database/MusicDatabase.h"
#include <mutex>
#include <taglib/tstring.h>
#include <unordered_map>

class MetadataService {
public:
  static MetadataService &getInstance();

  bool extractMetadata(const std::string &filePath, MusicMetadata &metadata);
  bool extractMetadataWithTagEditor(const std::string &filePath,
                                    MusicMetadata &metadata);
  bool updateFileTagsInternal(const std::string &filePath,
                              const MusicMetadata &metadata);
  bool extractAlbumArt(const std::string &filePath,
                       std::vector<char> &albumArt);

  MusicMetadata *getFromCache(const std::string &filePath);
  void addToCache(const std::string &filePath, const MusicMetadata &metadata);
  void clearCacheForPath(const std::string &filePath);

  std::string fixTagLibString(const TagLib::String &str);

private:
  MetadataService() = default;

  struct CachedMetadata {
    MusicMetadata metadata;
    std::chrono::steady_clock::time_point lastAccess;
  };

  std::unordered_map<std::string, CachedMetadata> cache_;
  std::mutex cacheMutex_;
  static constexpr size_t MAX_CACHE_SIZE = 500;

  void cleanupCache();
};
