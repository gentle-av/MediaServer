#include "services/music/MetadataExtractor.h"
#include "tagger/TagEditor.h"
#include <algorithm>
#include <filesystem>
#include <regex>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/tstring.h>

namespace fs = std::filesystem;

std::string MetadataExtractor::fixTagLibString(const TagLib::String &str) {
  return str.to8Bit(true);
}

bool MetadataExtractor::extractWithTagLib(const std::string &filePath,
                                          MusicMetadata &metadata) {
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      metadata.title = fixTagLibString(tag->title());
      metadata.artist = fixTagLibString(tag->artist());
      metadata.album = fixTagLibString(tag->album());
      metadata.genre = fixTagLibString(tag->genre());
      metadata.track = tag->track();
      metadata.year = tag->year();
      metadata.duration =
          f.audioProperties() ? f.audioProperties()->lengthInSeconds() : 0;
      return true;
    }
  } catch (const std::exception &) {
  }
  return false;
}

bool MetadataExtractor::extractFilenameFallback(const std::string &filePath,
                                                MusicMetadata &metadata) {
  std::string filename = fs::path(filePath).stem().string();
  std::regex trackPrefix(R"(^\s*\d{1,3}[\.\-\s]+\s*)");
  filename = std::regex_replace(filename, trackPrefix, "");
  metadata.title = filename.empty() ? "Unknown" : filename;
  metadata.artist = "Unknown";
  metadata.album = "Unknown";
  metadata.duration = 0;
  metadata.track = 0;
  metadata.year = 0;
  return true;
}

bool MetadataExtractor::extractWithTagEditor(const std::string &filePath,
                                             MusicMetadata &metadata) {
  std::string ext = fs::path(filePath).extension().string();
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
    }
    metadata.duration = 0;
    return true;
  } catch (const std::exception &) {
  }
  return false;
}

bool MetadataExtractor::extractMetadata(const std::string &filePath,
                                        MusicMetadata &metadata) {
  if (!fs::exists(filePath))
    return false;
  bool success = extractWithTagLib(filePath, metadata);
  if (!success)
    success = extractWithTagEditor(filePath, metadata);
  if (!success)
    success = extractFilenameFallback(filePath, metadata);
  if (metadata.title.empty()) {
    std::string filename = fs::path(filePath).stem().string();
    metadata.title = filename.empty() ? "Unknown" : filename;
  }
  return success && metadata.duration > 0;
}

bool MetadataExtractor::extractFlacAlbumArt(const std::string &filePath,
                                            std::vector<char> &albumArt) {
  try {
    TagLib::FLAC::File flacFile(filePath.c_str());
    if (!flacFile.isOpen())
      return false;
    auto pictures = flacFile.pictureList();
    if (pictures.isEmpty())
      return false;
    TagLib::FLAC::Picture *bestPicture = nullptr;
    for (auto picture : pictures) {
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
  } catch (const std::exception &) {
  }
  return false;
}

bool MetadataExtractor::extractAlbumArt(const std::string &filePath,
                                        std::vector<char> &albumArt) {
  std::string ext = fs::path(filePath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == ".flac")
    return extractFlacAlbumArt(filePath, albumArt);
  return false;
}

bool MetadataExtractor::updateFileTags(const std::string &filePath,
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
  } catch (const std::exception &) {
  }
  return false;
}
