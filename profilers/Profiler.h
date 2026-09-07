#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ProfileConfig {
  std::string name;
  int port;
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
  std::string databasePath;
  std::string videoDirectory;
};

class Profiler {
public:
  Profiler(int argc, char *argv[]);
  ~Profiler() = default;

  ProfileConfig getConfig() const { return config; }
  std::string getIndexPath() const { return config.indexPath; }
  std::string getDocumentRoot() const { return config.documentRoot; }
  std::string getHtmlPath() const { return config.htmlPath; }
  std::string getDatabasePath() const { return config.databasePath; }
  std::string getMusicDirectory() const { return config.musicDirectory; }
  std::string getVideoDirectory() const { return config.videoDirectory; }
  int getPort() const { return config.port; }

  void printStartupInfo() const;

private:
  ProfileConfig config;

  void initializeConfiguration();
  void parseCommandLine(int argc, char *argv[]);
  void loadConfigurationFromFile();
  void applyConfigDefaults();
  void setDefaultConfigValues();
  void printHelp(const char *programName) const;
  bool loadConfigFromFile(const fs::path &configPath);
  void parseConfigJson(const nlohmann::json &fullConfig);
  void extractConfigValues(const nlohmann::json &profileConfig);
  void validateDocumentRoot();
  void findIndexFile();
  bool findIndexFileInPaths(const std::vector<fs::path> &paths,
                            fs::path &foundPath);
  void validateIndexFile();
  void logSearchPaths(const std::vector<fs::path> &paths) const;

  fs::path findConfigFile() const;
  std::vector<fs::path> getIndexSearchPaths() const;
  std::vector<fs::path> getConfigSearchPaths() const;

  size_t parseBodySize(const std::string &sizeStr) const;
  void parseHeaderString(
      const std::string &headerStr,
      std::vector<std::pair<std::string, std::string>> &headers) const;
};
