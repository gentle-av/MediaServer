#pragma once

#include <string>
#include <vector>

struct Book {
  int id = 0;
  std::string filePath;
  std::string title;
  std::string author;
  std::string genre;
  std::string language;
  std::string sequence;
  int sequenceNumber = 0;
  std::vector<char> coverData;
  std::string coverMime;
};
