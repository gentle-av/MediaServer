#pragma once

#include <string>

class VideoIntegrityChecker {
public:
  enum class Status {
    Valid,
    Corrupted,
    EmptyFile,
    FileNotFound,
    UnsupportedFormat,
    ReadError
  };

  static Status check(const std::string &filepath,
                      std::string *errorMsg = nullptr);
  static std::string statusToString(Status status);

private:
  static Status checkMagicBytes(const std::string &filepath);
  static Status checkContainerStructure(const std::string &filepath);
  static Status checkReadability(const std::string &filepath);
  static Status checkSizeConsistency(const std::string &filepath);
};
