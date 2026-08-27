#pragma once

#include "DirectoryWatcher.h"
#include "database/ImageDatabase.h"
#include <filesystem>
#include <memory>
#include <string>

class NewVideoThumbnailExtractor {
public:
  NewVideoThumbnailExtractor(const std::string &watchDirectory,
                             ImageDatabase &database);
  ~NewVideoThumbnailExtractor();

  void start();
  void stop();
  bool isRunning() const;

private:
  void onVideoAdded(const std::filesystem::path &videoPath);
  void processVideo(const std::filesystem::path &videoPath);

  std::string watchDirectory;
  ImageDatabase &database;
  std::unique_ptr<DirectoryWatcher> watcher;
  bool running;
};
