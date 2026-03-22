// MusicMetadataExtractor.cpp
#include "MusicMetadataExtractor.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

MusicMetadata
MusicMetadataExtractor::extractMetadata(const std::string &filePath) {
  MusicMetadata metadata;
  metadata.filePath = filePath;
  try {
    TagLib::FileRef file(filePath.c_str());
    if (!file.isNull() && file.tag()) {
      TagLib::Tag *tag = file.tag();
      metadata.title = tag->title().to8Bit(true);
      metadata.artist = tag->artist().to8Bit(true);
      metadata.album = tag->album().to8Bit(true);
      metadata.genre = tag->genre().to8Bit(true);
      metadata.year = tag->year();
      metadata.trackNumber = tag->track();
      metadata.isValid = !metadata.artist.empty() || !metadata.album.empty();
      if (metadata.artist.empty() &&
          file.file()->properties().contains("ARTIST")) {
        metadata.artist =
            file.file()->properties()["ARTIST"].toString().to8Bit(true);
      }
      if (metadata.album.empty() &&
          file.file()->properties().contains("ALBUM")) {
        metadata.album =
            file.file()->properties()["ALBUM"].toString().to8Bit(true);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error extracting metadata from " << filePath << ": "
              << e.what() << std::endl;
  }
  return metadata;
}

bool MusicMetadataExtractor::isAudioFile(const std::string &path) {
  static std::vector<std::string> audioExts = {
      ".mp3", ".flac", ".wav", ".aac", ".ogg", ".m4a", ".wma", ".opus"};
  fs::path filePath(path);
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end();
}

void MusicMetadataExtractor::scanDirectory(const fs::path &path,
                                           std::vector<MusicMetadata> &results,
                                           int maxDepth) {
  if (maxDepth <= 0)
    return;
  try {
    for (const auto &entry : fs::directory_iterator(path)) {
      if (entry.is_directory()) {
        scanDirectory(entry.path(), results, maxDepth - 1);
      } else if (entry.is_regular_file() &&
                 isAudioFile(entry.path().string())) {
        MusicMetadata metadata = extractMetadata(entry.path().string());
        if (metadata.isValid) {
          results.push_back(metadata);
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error scanning directory " << path << ": " << e.what()
              << std::endl;
  }
}

std::vector<std::string>
MusicMetadataExtractor::getArtists(const std::string &rootPath) {
  std::vector<MusicMetadata> allMetadata;
  scanDirectory(rootPath, allMetadata);
  std::set<std::string> uniqueArtists;
  for (const auto &metadata : allMetadata) {
    if (!metadata.artist.empty()) {
      uniqueArtists.insert(metadata.artist);
    }
  }
  return std::vector<std::string>(uniqueArtists.begin(), uniqueArtists.end());
}

std::vector<std::pair<std::string, std::string>>
MusicMetadataExtractor::getAlbumsByArtist(const std::string &rootPath,
                                          const std::string &artist) {
  std::vector<MusicMetadata> allMetadata;
  scanDirectory(rootPath, allMetadata);
  std::set<std::pair<std::string, std::string>> uniqueAlbums;
  for (const auto &metadata : allMetadata) {
    if (metadata.artist == artist && !metadata.album.empty()) {
      fs::path albumPath = fs::path(metadata.filePath).parent_path();
      uniqueAlbums.insert({metadata.album, albumPath.string()});
    }
  }
  return std::vector<std::pair<std::string, std::string>>(uniqueAlbums.begin(),
                                                          uniqueAlbums.end());
}

std::vector<std::tuple<std::string, std::string, std::string>>
MusicMetadataExtractor::getAllAlbums(const std::string &rootPath) {
  std::vector<MusicMetadata> allMetadata;
  scanDirectory(rootPath, allMetadata);
  std::set<std::tuple<std::string, std::string, std::string>> uniqueAlbums;
  for (const auto &metadata : allMetadata) {
    if (!metadata.album.empty() && !metadata.artist.empty()) {
      fs::path albumPath = fs::path(metadata.filePath).parent_path();
      uniqueAlbums.insert(
          {metadata.artist, metadata.album, albumPath.string()});
    }
  }
  return std::vector<std::tuple<std::string, std::string, std::string>>(
      uniqueAlbums.begin(), uniqueAlbums.end());
}
