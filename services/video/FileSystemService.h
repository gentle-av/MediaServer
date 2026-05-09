#pragma once

#include <json/json.h>
#include <string>
#include <vector>

class FileSystemService {
public:
  static FileSystemService &getInstance();
  Json::Value listDirectory(const std::string &path);
  bool moveToTrash(const std::string &path);
  bool deleteDirectory(const std::string &path);
  bool isVideoFile(const std::string &filename);
  std::string formatFileSize(uintmax_t size);
  std::string getIconForFile(const std::string &ext);
  bool isPathAllowed(const std::string &path);
  bool fileExists(const std::string &path);
  bool isDirectory(const std::string &path);

private:
  FileSystemService() = default;
  std::vector<std::string> videoExtensions = {".mp4", ".avi", ".mkv",  ".mov",
                                              ".wmv", ".flv", ".webm", ".m4v",
                                              ".mpg", ".mpeg"};
};
