#pragma once

#include <string>
#include <vector>

class StaticFileService {
public:
  static StaticFileService &getInstance();
  std::string serveIndex(const std::string &indexPath);
  std::string serveStaticFile(const std::string &basePath,
                              const std::string &filename);
  std::string getContentType(const std::string &filename);

private:
  StaticFileService() = default;
  std::string findStaticFile(const std::string &basePath,
                             const std::string &filename);
  std::string readFileContent(const std::string &filePath);
  bool fileExists(const std::string &path);
};
