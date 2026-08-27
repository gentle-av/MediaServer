#include "StaticFileService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

StaticFileService &StaticFileService::getInstance() {
  static StaticFileService instance;
  return instance;
}

bool StaticFileService::fileExists(const std::string &path) {
  return fs::exists(path) && fs::is_regular_file(path);
}

std::string StaticFileService::readFileContent(const std::string &filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string StaticFileService::getContentType(const std::string &filename) {
  std::string ext;
  size_t pos = filename.rfind('.');
  if (pos != std::string::npos) {
    ext = filename.substr(pos);
  }
  if (ext == ".html" || ext == ".htm")
    return "text/html";
  if (ext == ".css")
    return "text/css";
  if (ext == ".js")
    return "application/javascript";
  if (ext == ".json")
    return "application/json";
  if (ext == ".png")
    return "image/png";
  if (ext == ".jpg" || ext == ".jpeg")
    return "image/jpeg";
  if (ext == ".gif")
    return "image/gif";
  if (ext == ".svg")
    return "image/svg+xml";
  if (ext == ".ico")
    return "image/x-icon";
  if (ext == ".txt")
    return "text/plain";
  if (ext == ".pdf")
    return "application/pdf";
  if (ext == ".xml")
    return "application/xml";
  if (ext == ".zip")
    return "application/zip";
  if (ext == ".mp4")
    return "video/mp4";
  if (ext == ".webm")
    return "video/webm";
  if (ext == ".mp3")
    return "audio/mpeg";
  return "application/octet-stream";
}

std::string StaticFileService::findStaticFile(const std::string &basePath,
                                              const std::string &filename) {
  std::vector<std::string> searchPaths;
  if (!basePath.empty()) {
    searchPaths.push_back(basePath + "/" + filename);
  }
  searchPaths.push_back("./views/" + filename);
  searchPaths.push_back("/usr/local/share/html/views/" + filename);
  searchPaths.push_back("/home/avr/.local/html/MediaServer/test/views/" +
                        filename);
  for (const auto &path : searchPaths) {
    if (fileExists(path)) {
      return path;
    }
  }
  return "";
}

std::string StaticFileService::serveIndex(const std::string &indexPath) {
  if (indexPath.empty()) {
    return "<html><body><h1>404</h1><p>index.html not found in "
           "configuration</p></body></html>";
  }
  std::string content = readFileContent(indexPath);
  if (content.empty()) {
    return "<html><body><h1>404</h1><p>Cannot open index.html at: " +
           indexPath + "</p></body></html>";
  }
  return content;
}

std::string StaticFileService::serveStaticFile(const std::string &basePath,
                                               const std::string &filename) {
  std::string filePath = findStaticFile(basePath, filename);
  if (!filePath.empty()) {
    std::string content = readFileContent(filePath);
    if (!content.empty()) {
      return content;
    }
  }
  return "<html><body><h1>404</h1><p>File not found: " + filename +
         "</p></body></html>";
}
