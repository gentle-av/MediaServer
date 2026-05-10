#pragma once

#include <string>
#include <vector>

class AlbumArtWriter {
public:
  bool writeToFile(const std::string &filePath,
                   const std::vector<char> &imageData);
  bool removeFromFile(const std::string &filePath);

private:
  bool writeToFlac(const std::string &filePath,
                   const std::vector<char> &imageData);
  std::string detectMimeType(const std::vector<char> &data);
};
