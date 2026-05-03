#include "services/MetadataService.h"
#include "tagger/TagEditor.h"
#include <filesystem>
#include <regex>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>

namespace fs = std::filesystem;

MetadataService &MetadataService::getInstance() {
  static MetadataService instance;
  return instance;
}

bool MetadataService::extractMetadata(const std::string &filePath,
                                      MusicMetadata &metadata) {
  MusicMetadata *cached = getFromCache(filePath);
  if (cached) {
    metadata = *cached;
    return true;
  }
  if (!fs::exists(filePath)) {
    return false;
  }
  bool success = false;
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      metadata.title = fixTagLibString(tag->title());
      metadata.artist = fixTagLibString(tag->artist());
      metadata.album = fixTagLibString(tag->album());
      metadata.genre = fixTagLibString(tag->genre());
      if (metadata.title.empty()) {
        std::string filename = fs::path(filePath).stem().string();
        std::regex trackPrefix(R"(^\s*\d{1,3}[\.\-\s]+\s*)");
        metadata.title = std::regex_replace(filename, trackPrefix, "");
        if (metadata.title.find_last_of('.') != std::string::npos) {
          metadata.title =
              metadata.title.substr(0, metadata.title.find_last_of('.'));
        }
      }
      if (metadata.title.empty())
        metadata.title = "Unknown";
      if (metadata.artist.empty())
        metadata.artist = "Unknown";
      if (metadata.album.empty())
        metadata.album = "Unknown";
      if (f.audioProperties()) {
        metadata.duration = f.audioProperties()->lengthInSeconds();
      } else {
        metadata.duration = 0;
      }
      metadata.track = tag->track();
      metadata.year = tag->year();
      success = true;
    }
  } catch (const std::exception &e) {
  }
  if (!success) {
    std::string filename = fs::path(filePath).stem().string();
    std::regex trackPrefix(R"(^\s*\d{1,3}[\.\-\s]+\s*)");
    filename = std::regex_replace(filename, trackPrefix, "");
    metadata.title = filename.empty() ? "Unknown" : filename;
    metadata.artist = "Unknown";
    metadata.album = "Unknown";
    metadata.duration = 0;
    metadata.track = 0;
    metadata.year = 0;
    success = true;
  }
  if (success) {
    addToCache(filePath, metadata);
  }
  return success;
}

MusicMetadata *MetadataService::getFromCache(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  auto it = cache_.find(filePath);
  if (it != cache_.end()) {
    it->second.lastAccess = std::chrono::steady_clock::now();
    return &it->second.metadata;
  }
  return nullptr;
}

void MetadataService::addToCache(const std::string &filePath,
                                 const MusicMetadata &metadata) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  if (cache_.size() >= MAX_CACHE_SIZE) {
    cleanupCache();
  }
  CachedMetadata cached;
  cached.metadata = metadata;
  cached.lastAccess = std::chrono::steady_clock::now();
  cache_[filePath] = cached;
}

void MetadataService::clearCacheForPath(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  cache_.erase(filePath);
}

void MetadataService::cleanupCache() {
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
  if (cache_.size() > MAX_CACHE_SIZE) {
    size_t toErase = cache_.size() - MAX_CACHE_SIZE;
    auto it = cache_.begin();
    for (size_t i = 0; i < toErase && it != cache_.end(); ++i) {
      it = cache_.erase(it);
    }
  }
}

bool MetadataService::extractAlbumArt(const std::string &filePath,
                                      std::vector<char> &albumArt) {
  try {
    std::string ext = filePath.substr(filePath.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".flac")
      return false;
    TagLib::FLAC::File flacFile(filePath.c_str());
    if (!flacFile.isOpen())
      return false;
    auto pictures = flacFile.pictureList();
    if (pictures.isEmpty())
      return false;
    TagLib::FLAC::Picture *bestPicture = nullptr;
    for (auto it = pictures.begin(); it != pictures.end(); ++it) {
      TagLib::FLAC::Picture *picture = *it;
      if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
        bestPicture = picture;
        break;
      }
      if (!bestPicture)
        bestPicture = picture;
    }
    if (!bestPicture)
      return false;
    TagLib::ByteVector data = bestPicture->data();
    albumArt.assign(data.data(), data.data() + data.size());
    return true;
  } catch (const std::exception &e) {
  }
  return false;
}

bool MetadataService::extractMetadataWithTagEditor(const std::string &filePath,
                                                   MusicMetadata &metadata) {
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".flac")
    return false;
  try {
    TagEditor editor(filePath);
    if (!editor.load())
      return false;
    metadata.title = editor.getTitle();
    metadata.artist = editor.getArtist();
    metadata.album = editor.getAlbum();
    metadata.genre = editor.getGenre();
    metadata.track = editor.getTrackNumber();
    std::string date = editor.getDate();
    if (!date.empty() && date.length() >= 4) {
      try {
        metadata.year = std::stoi(date.substr(0, 4));
      } catch (...) {
        metadata.year = 0;
      }
    } else {
      metadata.year = 0;
    }
    metadata.duration = 0;
    return true;
  } catch (const std::exception &e) {
  }
  return false;
}

bool MetadataService::updateFileTagsInternal(const std::string &filePath,
                                             const MusicMetadata &metadata) {
  std::string ext = fs::path(filePath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".flac")
    return false;
  try {
    TagEditor editor(filePath);
    if (!editor.load())
      return false;
    if (!metadata.title.empty())
      editor.setTitle(metadata.title);
    if (!metadata.artist.empty())
      editor.setArtist(metadata.artist);
    if (!metadata.album.empty())
      editor.setAlbum(metadata.album);
    if (!metadata.genre.empty())
      editor.setGenre(metadata.genre);
    if (metadata.track > 0)
      editor.setTrackNumber(metadata.track);
    if (metadata.year > 0)
      editor.setDate(std::to_string(metadata.year));
    return editor.save();
  } catch (const std::exception &e) {
  }
  return false;
}

std::string MetadataService::fixTagLibString(const TagLib::String &str) {
  return str.to8Bit(true);
}
