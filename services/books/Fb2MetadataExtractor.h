#pragma once
#include "../../models/Book.h"
#include <string>

class Fb2MetadataExtractor {
public:
  static bool extractFull(const std::string &filePath, Book &book);

private:
  static std::string extractTagContent(const std::string &xml,
                                       const std::string &tagName);
  static std::vector<char> decodeBase64(const std::string &encoded);
  static std::string detectXmlEncoding(const std::string &xmlHead);
  static std::string convertToUtf8(const std::string &input,
                                   const std::string &fromEncoding);
  static bool isUtf8(const std::string &s);
};
