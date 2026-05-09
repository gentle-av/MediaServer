#pragma once

#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <string>

class StaticFileService {
public:
  static StaticFileService &getInstance();
  drogon::HttpResponsePtr serveIndex(const std::string &indexPath);
  drogon::HttpResponsePtr serveStaticFile(const std::string &basePath,
                                          const std::string &filename);

private:
  StaticFileService() = default;
  std::string findStaticFile(const std::string &basePath,
                             const std::string &filename);
};
