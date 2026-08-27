#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct ImageData {
  std::vector<unsigned char> data;
  std::string mimeType;
};

class ImageDatabase {
public:
  explicit ImageDatabase(const std::string &dbPath);
  ~ImageDatabase();

  bool init();
  void close();

  bool saveImage(const std::string &filePath, const ImageData &image);
  std::optional<ImageData> getImage(const std::string &filePath);
  bool removeImage(const std::string &filePath);
  bool imageExists(const std::string &filePath);
  std::vector<std::string> getAllPaths();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
