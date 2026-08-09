#pragma once

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
  std::string databasePath;
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
  std::string getDatabasePath() const { return config_.databasePath; }
  std::string getMusicDirectory() const { return config_.musicDirectory; }
  int getPlayerPort() const { return config_.playerPort; }
  void printStartupInfo() const;

private:
  ProfileConfig config_;
  nlohmann::json drogonConfig_;
  void initializeConfiguration();
  void parseCommandLine(int argc, char *argv[]);
  void loadConfigurationFromFile();
  void applyConfigDefaults();
  void setDefaultConfigValues();
  void printHelp(const char *programName) const;
  bool loadConfigFromFile(const fs::path &configPath);
  void parseConfigJson(const nlohmann::json &fullConfig);
  void extractConfigValues();
  void validateDocumentRoot();
  void findIndexFile();
  bool findIndexFileInPaths(const std::vector<fs::path> &paths,
                            fs::path &foundPath);
  void validateIndexFile();
  void logSearchPaths(const std::vector<fs::path> &paths) const;
  void setupDrogonConfig();
  void setupDocumentRoot();
  void setupListeners();
  void setupAppConfig();
  fs::path findConfigFile() const;
  std::vector<fs::path> getIndexSearchPaths() const;
  std::vector<fs::path> getConfigSearchPaths() const;
  size_t parseBodySize(const std::string &sizeStr) const;
  void parseHeaderString(
      const std::string &headerStr,
      std::vector<std::pair<std::string, std::string>> &headers) const;
};
