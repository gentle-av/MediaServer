#include "VideoIntegrityChecker.h"
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

VideoIntegrityChecker::Status
VideoIntegrityChecker::check(const std::string &filepath,
                             std::string *errorMsg) {
  if (!fs::exists(filepath)) {
    if (errorMsg)
      *errorMsg = "File not found: " + filepath;
    return Status::FileNotFound;
  }
  auto fileSize = fs::file_size(filepath);
  if (fileSize == 0) {
    if (errorMsg)
      *errorMsg = "File is empty";
    return Status::EmptyFile;
  }
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    if (errorMsg)
      *errorMsg = "Cannot open file";
    return Status::ReadError;
  }
  auto magicStatus = checkMagicBytes(filepath);
  if (magicStatus != Status::Valid) {
    if (errorMsg)
      *errorMsg = "Invalid file signature";
    return magicStatus;
  }
  auto containerStatus = checkContainerStructure(filepath);
  if (containerStatus != Status::Valid) {
    if (errorMsg)
      *errorMsg = "Invalid container structure";
    return containerStatus;
  }
  auto readabilityStatus = checkReadability(filepath);
  if (readabilityStatus != Status::Valid) {
    if (errorMsg)
      *errorMsg = "Cannot read essential data blocks";
    return readabilityStatus;
  }
  auto sizeStatus = checkSizeConsistency(filepath);
  if (sizeStatus != Status::Valid) {
    if (errorMsg)
      *errorMsg = "Size inconsistency detected";
    return sizeStatus;
  }
  return Status::Valid;
}

std::string VideoIntegrityChecker::statusToString(Status status) {
  switch (status) {
  case Status::Valid:
    return "Valid";
  case Status::Corrupted:
    return "Corrupted";
  case Status::EmptyFile:
    return "Empty file";
  case Status::FileNotFound:
    return "File not found";
  case Status::UnsupportedFormat:
    return "Unsupported format";
  case Status::ReadError:
    return "Read error";
  }
  return "Unknown";
}

VideoIntegrityChecker::Status
VideoIntegrityChecker::checkMagicBytes(const std::string &filepath) {
  struct MagicSignature {
    std::vector<uint8_t> bytes;
    size_t offset;
    std::string format;
  };
  std::vector<MagicSignature> signatures = {
      {{0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70}, 0, "MP4/MOV"},
      {{0x1A, 0x45, 0xDF, 0xA3}, 0, "Matroska/MKV"},
      {{0x52, 0x49, 0x46, 0x46}, 0, "AVI"},
      {{0x00, 0x00, 0x01, 0xBA}, 0, "MPEG-PS"},
      {{0x47, 0x40}, 0, "MPEG-TS"},
      {{0x46, 0x4C, 0x56}, 0, "FLV"},
      {{0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11}, 0, "ASF/WMV"},
      {{0x89, 0x49, 0x44, 0x58, 0x33}, 0, "IDX3/AVI"},
      {{0x4F, 0x67, 0x67, 0x53}, 0, "OGG/Theora"},
      {{0x66, 0x74, 0x79, 0x70, 0x6D, 0x70, 0x34, 0x32}, 4, "MP4"}};
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    return Status::ReadError;
  }
  std::vector<uint8_t> header(64);
  file.read(reinterpret_cast<char *>(header.data()), header.size());
  if (file.gcount() < 8) {
    return Status::Corrupted;
  }
  for (const auto &sig : signatures) {
    if (file.gcount() < sig.offset + sig.bytes.size()) {
      continue;
    }
    bool match = true;
    for (size_t i = 0; i < sig.bytes.size(); ++i) {
      if (header[sig.offset + i] != sig.bytes[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      return Status::Valid;
    }
  }
  return Status::UnsupportedFormat;
}

VideoIntegrityChecker::Status
VideoIntegrityChecker::checkContainerStructure(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    return Status::ReadError;
  }
  uint8_t buffer[4];
  file.read(reinterpret_cast<char *>(buffer), 4);
  if (file.gcount() != 4) {
    return Status::Corrupted;
  }
  if (buffer[4] == 0x66 && buffer[5] == 0x74 && buffer[6] == 0x79 &&
      buffer[7] == 0x70) {
    file.seekg(0);
    uint32_t boxSize;
    file.read(reinterpret_cast<char *>(&boxSize), 4);
    boxSize = std::byteswap(boxSize);
    auto fileSize = fs::file_size(filepath);
    if (boxSize < 8 || boxSize > fileSize) {
      return Status::Corrupted;
    }
  }
  if (buffer[0] == 0x1A && buffer[1] == 0x45 && buffer[2] == 0xDF &&
      buffer[3] == 0xA3) {
    file.seekg(4);
    uint8_t nextByte;
    file.read(reinterpret_cast<char *>(&nextByte), 1);
    if ((nextByte & 0x80) != 0x80) {
      return Status::Corrupted;
    }
  }
  return Status::Valid;
}

VideoIntegrityChecker::Status
VideoIntegrityChecker::checkReadability(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    return Status::ReadError;
  }
  auto fileSize = fs::file_size(filepath);
  std::array<uint64_t, 3> positions = {
      0, fileSize / 2, fileSize > 1024 ? fileSize - 1024 : fileSize / 2};
  std::vector<uint8_t> buffer(4096);
  bool canRead = false;
  for (auto pos : positions) {
    if (pos < fileSize) {
      file.clear();
      file.seekg(pos);
      file.read(reinterpret_cast<char *>(buffer.data()),
                std::min<size_t>(4096, fileSize - pos));
      if (file.gcount() > 0) {
        canRead = true;
        break;
      }
    }
  }
  return canRead ? Status::Valid : Status::Corrupted;
}

VideoIntegrityChecker::Status
VideoIntegrityChecker::checkSizeConsistency(const std::string &filepath) {
  auto fileSize = fs::file_size(filepath);
  constexpr uint64_t minVideoSize = 1024;
  constexpr uint64_t maxReasonableSize = 100ULL * 1024 * 1024 * 1024;
  if (fileSize < minVideoSize || fileSize > maxReasonableSize) {
    return Status::Corrupted;
  }
  return Status::Valid;
}
