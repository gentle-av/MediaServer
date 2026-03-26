#include "AlbumArtExtractor.h"
#include <algorithm>
#include <iostream>

std::unique_ptr<AlbumArtExtractor::AlbumArt>
AlbumArtExtractor::extractAlbumArt(const std::string &filePath) {
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == ".flac") {
    return extractFromFlac(filePath);
  }
  return nullptr;
}

bool AlbumArtExtractor::isSupportedFormat(const std::string &filePath) {
  std::string ext = filePath.substr(filePath.find_last_of("."));
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return (ext == ".flac");
}

std::unique_ptr<AlbumArtExtractor::AlbumArt>
AlbumArtExtractor::extractFromFlac(const std::string &filePath) {
  TagLib::FLAC::File file(filePath.c_str());
  if (!file.isOpen()) {
    std::cerr << "Cannot open FLAC file: " << filePath << std::endl;
    return nullptr;
  }
  auto pictures = file.pictureList();
  std::cerr << "Found " << pictures.size() << " pictures in " << filePath
            << std::endl;
  if (pictures.isEmpty()) {
    return nullptr;
  }
  TagLib::FLAC::Picture *bestPicture = nullptr;
  for (auto it = pictures.begin(); it != pictures.end(); ++it) {
    TagLib::FLAC::Picture *picture = *it;
    if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
      bestPicture = picture;
      break;
    }
    if (!bestPicture) {
      bestPicture = picture;
    }
  }
  if (!bestPicture) {
    return nullptr;
  }
  auto art = std::make_unique<AlbumArt>();
  TagLib::ByteVector data = bestPicture->data();
  art->data.assign(data.data(), data.data() + data.size());
  art->mimeType = bestPicture->mimeType().toCString();
  if (art->mimeType.empty()) {
    art->mimeType = detectImageType(art->data);
  }
  art->description = bestPicture->description().toCString();
  return art;
}

std::string AlbumArtExtractor::detectImageType(const std::vector<char> &data) {
  if (data.size() < 8) {
    return "application/octet-stream";
  }
  if (data[0] == 0xFF && data[1] == 0xD8) {
    return "image/jpeg";
  }
  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E &&
      data[3] == 0x47 && data[4] == 0x0D && data[5] == 0x0A &&
      data[6] == 0x1A && data[7] == 0x0A) {
    return "image/png";
  }
  if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46) {
    return "image/gif";
  }
  if (data[0] == 0x42 && data[1] == 0x4D) {
    return "image/bmp";
  }
  return "application/octet-stream";
}

std::string
AlbumArtExtractor::getMimeTypeFromData(const std::vector<char> &data) {
  return detectImageType(data);
}
