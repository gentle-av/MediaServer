// TagEditor.cpp
#include "TagEditor.h"
#include <algorithm>
#include <iostream>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/xiphcomment.h>

TagEditor::TagEditor(const std::string &filename)
    : filename_(filename), modified_(false), loaded_(false) {}

TagEditor::~TagEditor() = default;

TagEditor::FileType TagEditor::detectFileType() const {
  size_t dotPos = filename_.find_last_of('.');
  if (dotPos == std::string::npos)
    return FileType::Unknown;
  std::string ext = filename_.substr(dotPos);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == ".mp3")
    return FileType::MP3;
  if (ext == ".flac")
    return FileType::FLAC;
  return FileType::Unknown;
}

bool TagEditor::load() {
  std::cout << "[TagEditor] Loading file: " << filename_ << std::endl;
  FileType type = detectFileType();
  if (type == FileType::Unknown) {
    std::cout << "[TagEditor] Unknown file type" << std::endl;
    return false;
  }
  try {
    if (type == FileType::MP3) {
      TagLib::MPEG::File file(filename_.c_str());
      if (!file.isOpen()) {
        std::cout << "[TagEditor] Failed to open MP3 file" << std::endl;
        return false;
      }
      TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
      if (tag) {
        tags_.title = tag->title().to8Bit(true);
        tags_.artist = tag->artist().to8Bit(true);
        tags_.album = tag->album().to8Bit(true);
        tags_.genre = tag->genre().to8Bit(true);
        tags_.track = tag->track();
        auto frameMap = tag->frameListMap();
        if (frameMap.contains("TDRC")) {
          auto frameList = frameMap["TDRC"];
          if (!frameList.isEmpty()) {
            auto frame = frameList.front();
            if (frame) {
              std::string date = frame->toString().to8Bit(true);
              if (date.length() >= 4) {
                tags_.date = date.substr(0, 4);
              }
            }
          }
        }
      }
    } else if (type == FileType::FLAC) {
      TagLib::FLAC::File file(filename_.c_str());
      if (!file.isOpen()) {
        std::cout << "[TagEditor] Failed to open FLAC file" << std::endl;
        return false;
      }
      TagLib::Tag *tag = file.tag();
      if (tag) {
        tags_.title = tag->title().to8Bit(true);
        tags_.artist = tag->artist().to8Bit(true);
        tags_.album = tag->album().to8Bit(true);
        tags_.genre = tag->genre().to8Bit(true);
        tags_.track = tag->track();
        if (tag->year() > 0) {
          tags_.date = std::to_string(tag->year());
        }
      }
      TagLib::Ogg::XiphComment *comment = file.xiphComment();
      if (comment) {
        const auto &fieldMap = comment->fieldListMap();
        auto dateIt = fieldMap.find("DATE");
        if (dateIt != fieldMap.end() && !dateIt->second.isEmpty()) {
          tags_.date = dateIt->second.front().to8Bit(true);
        }
        auto trackIt = fieldMap.find("TRACKNUMBER");
        if (trackIt != fieldMap.end() && !trackIt->second.isEmpty()) {
          std::string trackStr = trackIt->second.front().to8Bit(true);
          try {
            tags_.track = std::stoi(trackStr);
          } catch (...) {
          }
        }
      }
    }
    loaded_ = true;
    std::cout << "[TagEditor] Loaded: Title=" << tags_.title
              << ", Artist=" << tags_.artist << ", Album=" << tags_.album
              << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cout << "[TagEditor] Exception: " << e.what() << std::endl;
    return false;
  }
}

bool TagEditor::save() {
  if (!loaded_ && !load()) {
    std::cout << "[TagEditor] Cannot save - file not loaded" << std::endl;
    return false;
  }
  if (!modified_) {
    std::cout << "[TagEditor] No changes to save" << std::endl;
    return true;
  }
  std::cout << "[TagEditor] Saving file: " << filename_ << std::endl;
  FileType type = detectFileType();
  if (type == FileType::Unknown) {
    return false;
  }
  try {
    if (type == FileType::MP3) {
      TagLib::MPEG::File file(filename_.c_str());
      if (!file.isOpen())
        return false;
      TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
      if (!tag)
        return false;
      if (!tags_.title.empty())
        tag->setTitle(TagLib::String(tags_.title, TagLib::String::UTF8));
      if (!tags_.artist.empty())
        tag->setArtist(TagLib::String(tags_.artist, TagLib::String::UTF8));
      if (!tags_.album.empty())
        tag->setAlbum(TagLib::String(tags_.album, TagLib::String::UTF8));
      if (!tags_.genre.empty())
        tag->setGenre(TagLib::String(tags_.genre, TagLib::String::UTF8));
      if (tags_.track > 0)
        tag->setTrack(tags_.track);
      if (!tags_.date.empty() && tags_.date.length() >= 4) {
        try {
          tag->setYear(std::stoi(tags_.date.substr(0, 4)));
        } catch (...) {
        }
      }
      return file.save();
    } else if (type == FileType::FLAC) {
      TagLib::FLAC::File file(filename_.c_str());
      if (!file.isOpen())
        return false;
      TagLib::Tag *tag = file.tag();
      if (!tag)
        return false;
      if (!tags_.title.empty())
        tag->setTitle(TagLib::String(tags_.title, TagLib::String::UTF8));
      if (!tags_.artist.empty())
        tag->setArtist(TagLib::String(tags_.artist, TagLib::String::UTF8));
      if (!tags_.album.empty())
        tag->setAlbum(TagLib::String(tags_.album, TagLib::String::UTF8));
      if (!tags_.genre.empty())
        tag->setGenre(TagLib::String(tags_.genre, TagLib::String::UTF8));
      if (tags_.track > 0)
        tag->setTrack(tags_.track);
      if (!tags_.date.empty() && tags_.date.length() >= 4) {
        try {
          tag->setYear(std::stoi(tags_.date.substr(0, 4)));
        } catch (...) {
        }
      }
      TagLib::Ogg::XiphComment *comment = file.xiphComment(true);
      if (comment) {
        if (!tags_.date.empty()) {
          comment->addField(
              "DATE", TagLib::String(tags_.date, TagLib::String::UTF8), true);
        }
        if (tags_.track > 0) {
          comment->addField(
              "TRACKNUMBER",
              TagLib::String(std::to_string(tags_.track), TagLib::String::UTF8),
              true);
        }
      }
      return file.save();
    }
    return false;
  } catch (const std::exception &e) {
    std::cout << "[TagEditor] Save exception: " << e.what() << std::endl;
    return false;
  }
}

std::string TagEditor::getTitle() const { return tags_.title; }
std::string TagEditor::getArtist() const { return tags_.artist; }
std::string TagEditor::getAlbum() const { return tags_.album; }
std::string TagEditor::getDate() const { return tags_.date; }
std::string TagEditor::getGenre() const { return tags_.genre; }
int TagEditor::getTrackNumber() const { return tags_.track; }

void TagEditor::setTitle(const std::string &title) {
  if (tags_.title != title) {
    tags_.title = title;
    modified_ = true;
  }
}

void TagEditor::setArtist(const std::string &artist) {
  if (tags_.artist != artist) {
    tags_.artist = artist;
    modified_ = true;
  }
}

void TagEditor::setAlbum(const std::string &album) {
  if (tags_.album != album) {
    tags_.album = album;
    modified_ = true;
  }
}

void TagEditor::setDate(const std::string &date) {
  if (tags_.date != date) {
    tags_.date = date;
    modified_ = true;
  }
}

void TagEditor::setGenre(const std::string &genre) {
  if (tags_.genre != genre) {
    tags_.genre = genre;
    modified_ = true;
  }
}

void TagEditor::setTrackNumber(int track) {
  if (tags_.track != track) {
    tags_.track = track;
    modified_ = true;
  }
}
