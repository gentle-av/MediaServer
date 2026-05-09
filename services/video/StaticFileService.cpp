#include "StaticFileService.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

StaticFileService &StaticFileService::getInstance() {
  static StaticFileService instance;
  return instance;
}

drogon::HttpResponsePtr
StaticFileService::serveIndex(const std::string &indexPath) {
  if (indexPath.empty()) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k404NotFound);
    resp->setBody("index.html not found in configuration");
    return resp;
  }
  std::ifstream file(indexPath);
  if (!file.is_open()) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k404NotFound);
    resp->setBody("Cannot open index.html at: " + indexPath);
    return resp;
  }
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->setContentTypeCode(drogon::CT_TEXT_HTML);
  resp->setBody(content);
  return resp;
}

std::string StaticFileService::findStaticFile(const std::string &basePath,
                                              const std::string &filename) {
  std::vector<std::string> searchPaths;
  if (!basePath.empty()) {
    searchPaths.push_back(basePath + "/" + filename);
  }
  searchPaths.push_back("/home/avr/code/html/test/views/" + filename);
  searchPaths.push_back("/home/avr/code/html/product/views/" + filename);
  searchPaths.push_back("./views/" + filename);
  for (const auto &path : searchPaths) {
    if (fs::exists(path) && fs::is_regular_file(path)) {
      return path;
    }
  }
  return "";
}

drogon::HttpResponsePtr
StaticFileService::serveStaticFile(const std::string &basePath,
                                   const std::string &filename) {
  std::string filePath = findStaticFile(basePath, filename);
  if (!filePath.empty()) {
    return drogon::HttpResponse::newFileResponse(filePath);
  }
  Json::Value json;
  json["error"] = "File not found: " + filename;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
  resp->setStatusCode(drogon::k404NotFound);
  return resp;
}
