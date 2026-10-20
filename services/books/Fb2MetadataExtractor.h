#pragma once

#include "../../models/Book.h"
#include <string>

class Fb2MetadataExtractor {
public:
  static bool extractMetadata(const std::string &filePath, Book &book);

private:
  static std::string extractTagContent(const std::string &xml,
                                       const std::string &tagName);
  static std::vector<char> decodeBase64(const std::string &encoded);
};
