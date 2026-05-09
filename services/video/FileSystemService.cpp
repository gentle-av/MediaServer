#include "FileSystemService.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

FileSystemService &FileSystemService::getInstance() {
  static FileSystemService instance;
  return instance;
}

Json::Value FileSystemService::listDirectory(const std::string &path) {
  Json::Value result;
  std::vector<Json::Value> items;
  for (const auto &entry : fs::directory_iterator(path)) {
    Json::Value item;
    item["name"] = entry.path().filename().string();
    item["path"] = entry.path().string();
    item["isDirectory"] = entry.is_directory();
    if (entry.is_regular_file()) {
      item["size"] = formatFileSize(entry.file_size());
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      item["isVideo"] = isVideoFile(ext);
      item["icon"] = getIconForFile(ext);
    } else {
      item["icon"] = "folder";
      item["isVideo"] = false;
    }
    items.push_back(item);
  }
  std::sort(items.begin(), items.end(),
            [](const Json::Value &a, const Json::Value &b) {
              bool aIsDir = a["isDirectory"].asBool();
              bool bIsDir = b["isDirectory"].asBool();
              if (aIsDir != bIsDir)
                return aIsDir > bIsDir;
              return a["name"].asString() < b["name"].asString();
            });
  Json::Value itemsArray(Json::arrayValue);
  for (const auto &item : items) {
    itemsArray.append(item);
  }
  result["items"] = itemsArray;
  result["success"] = true;
  result["path"] = path;
  return result;
}

bool FileSystemService::moveToTrash(const std::string &path) {
  std::string trashCmd = "kioclient5 move \"" + path + "\" trash:/ 2>&1";
  return system(trashCmd.c_str()) == 0;
}

bool FileSystemService::deleteDirectory(const std::string &path) {
  std::string trashCmd = "kioclient5 move \"" + path + "\" trash:/ 2>&1";
  return system(trashCmd.c_str()) == 0;
}

bool FileSystemService::isVideoFile(const std::string &filename) {
  std::string lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return std::find(videoExtensions.begin(), videoExtensions.end(), lower) !=
         videoExtensions.end();
}

std::string FileSystemService::formatFileSize(uintmax_t size) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double fileSize = size;
  while (fileSize >= 1024 && unitIndex < 4) {
    fileSize /= 1024;
    unitIndex++;
  }
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.1f %s", fileSize, units[unitIndex]);
  return std::string(buffer);
}

std::string FileSystemService::getIconForFile(const std::string &ext) {
  if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" ||
      ext == ".wmv" || ext == ".flv" || ext == ".webm" || ext == ".m4v" ||
      ext == ".mpg" || ext == ".mpeg")
    return "video";
  if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" ||
      ext == ".bmp" || ext == ".svg")
    return "image";
  if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
      ext == ".m4a")
    return "audio";
  if (ext == ".pdf")
    return "pdf";
  if (ext == ".txt" || ext == ".md" || ext == ".log")
    return "text";
  return "file";
}

bool FileSystemService::isPathAllowed(const std::string &path) {
  return path.find("/mnt/video") == 0 || path.find("/mnt/media") == 0;
}

bool FileSystemService::fileExists(const std::string &path) {
  return fs::exists(path);
}

bool FileSystemService::isDirectory(const std::string &path) {
  return fs::is_directory(path);
}
