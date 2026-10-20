#pragma once

#include <string>

struct Bookmark {
  int id = 0;
  std::string bookPath;
  double position = 0.0;
  std::string progressText;
  std::string createdAt;
  std::string updatedAt;
};
