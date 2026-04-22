#pragma once

#include <drogon/drogon.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ProfileConfig {
  std::string name;
  int port;
  int playerPort;
  std::string address;
  std::string documentRoot;
  std::string indexPath;
  bool isTest;
  int threads;
  std::string logLevel;
  std::string logPath;
  std::string uploadPath;
  std::string musicDirectory;
  std::string htmlPath;
};

class Profiler {

public:
  Profiler(int argc, char *argv[]);
  ~Profiler() = default;
  ProfileConfig getConfig() const { return config_; }
  nlohmann::json getDrogonConfig() const { return drogonConfig_; }
  std::string getIndexPath() const { return config_.indexPath; }
  std::string getDocumentRoot() const { return config_.documentRoot; }
  std::string getHtmlPath() const { return config_.htmlPath; }
  int getPlayerPort() const { return config_.playerPort; }
  void applyToDrogon(drogon::HttpAppFramework &app) const;
  void printStartupInfo() const;

private:
  ProfileConfig config_;
  nlohmann::json drogonConfig_;
  void parseCommandLine(int argc, char *argv[]);
  void loadConfiguration();
  void findIndexFile();
  void setupDrogonConfig();
  fs::path findConfigFile() const;
  std::vector<fs::path> getIndexSearchPaths() const;
  std::vector<fs::path> getConfigSearchPaths() const;
  trantor::Logger::LogLevel stringToLogLevel(const std::string &level) const;
};
