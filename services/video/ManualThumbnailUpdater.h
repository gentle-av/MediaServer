#pragma once

#include "database/ImageDatabase.h"
#include <filesystem>

class ManualThumbnailUpdater {
public:
  explicit ManualThumbnailUpdater(ImageDatabase &database);
  ~ManualThumbnailUpdater() = default;

  bool updateThumbnail(const std::filesystem::path &videoPath);
  bool updateThumbnail(const std::filesystem::path &videoPath,
                       double timestampSeconds);

private:
  bool processVideo(const std::filesystem::path &videoPath,
                    double timestampSeconds);

  ImageDatabase &database;
};
