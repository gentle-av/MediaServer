#include "services/music/AlbumArtWriter.h"
#include <algorithm>
#include <iostream>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>

std::string AlbumArtWriter::detectMimeType(const std::vector<char> &data) {
  if (data.size() < 8)
    return "image/jpeg";
  if (data[0] == 0xFF && data[1] == 0xD8)
    return "image/jpeg";
  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
    return "image/png";
  if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46)
    return "image/gif";
  return "image/jpeg";
}

bool AlbumArtWriter::writeToFlac(const std::string &filePath,
                                 const std::vector<char> &imageData) {
  std::cerr << "[DEBUG] writeToFlac: " << filePath << std::endl;
  std::cerr << "[DEBUG] imageData size: " << imageData.size() << std::endl;
  if (imageData.empty()) {
    std::cerr << "[ERROR] Image data is empty" << std::endl;
    return false;
  }
  TagLib::FLAC::File file(filePath.c_str());
  if (!file.isOpen()) {
    std::cerr << "[ERROR] Cannot open file: " << filePath << std::endl;
    return false;
  }
  std::cerr << "[DEBUG] File opened successfully" << std::endl;
  TagLib::List<TagLib::FLAC::Picture *> pictures = file.pictureList();
  std::cerr << "[DEBUG] Existing pictures count: " << pictures.size()
            << std::endl;
  for (TagLib::FLAC::Picture *picture : pictures) {
    if (picture && picture->type() == TagLib::FLAC::Picture::FrontCover) {
      std::cerr << "[DEBUG] Removing existing FrontCover picture" << std::endl;
      file.removePicture(picture);
    }
  }
  TagLib::FLAC::Picture *newPicture = new TagLib::FLAC::Picture();
  std::string mimeType = detectMimeType(imageData);
  std::cerr << "[DEBUG] Detected mime type: " << mimeType << std::endl;
  newPicture->setType(TagLib::FLAC::Picture::FrontCover);
  newPicture->setMimeType(TagLib::String(mimeType, TagLib::String::UTF8));
  newPicture->setData(TagLib::ByteVector(imageData.data(), imageData.size()));
  newPicture->setDescription(TagLib::String("Cover", TagLib::String::UTF8));
  file.addPicture(newPicture);
  std::cerr << "[DEBUG] Saving file..." << std::endl;
  bool saved = file.save();
  std::cerr << "[DEBUG] Save result: " << saved << std::endl;
  return saved;
}

bool AlbumArtWriter::writeToFile(const std::string &filePath,
                                 const std::vector<char> &imageData) {
  std::cerr << "[DEBUG] writeToFile: " << filePath << std::endl;
  if (imageData.empty()) {
    std::cerr << "[ERROR] Image data is empty" << std::endl;
    return false;
  }
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  std::cerr << "[DEBUG] File extension: " << ext << std::endl;
  if (ext == ".flac") {
    return writeToFlac(filePath, imageData);
  }
  std::cerr << "[ERROR] Unsupported format: " << ext << std::endl;
  return false;
}

bool AlbumArtWriter::removeFromFile(const std::string &filePath) {
  std::cerr << "[DEBUG] removeFromFile: " << filePath << std::endl;
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".flac")
    return false;
  TagLib::FLAC::File file(filePath.c_str());
  if (!file.isOpen())
    return false;
  TagLib::List<TagLib::FLAC::Picture *> pictures = file.pictureList();
  for (TagLib::FLAC::Picture *picture : pictures) {
    if (picture && picture->type() == TagLib::FLAC::Picture::FrontCover) {
      file.removePicture(picture);
    }
  }
  return file.save();
}
