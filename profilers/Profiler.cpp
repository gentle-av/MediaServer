#include "profilers/Profiler.h"
#include <fstream>
#include <iostream>
#include <limits.h>
#include <unistd.h>

Profiler::Profiler(int argc, char *argv[]) {
  initializeConfiguration();
  parseCommandLine(argc, argv);
  loadConfigurationFromFile();
  applyConfigDefaults();
  findIndexFile();
  setupDrogonConfig();
}

void Profiler::initializeConfiguration() {
  setDefaultConfigValues();
  drogonConfig_ = nlohmann::json::object();
}

void Profiler::setDefaultConfigValues() {
  config_.name = "test";
  config_.isTest = true;
  config_.port = 8083;
  config_.playerPort = 9093;
  config_.address = "127.0.0.1";
  config_.threads = 2;
  config_.logLevel = "DEBUG";
  config_.logPath = "./logs";
  config_.uploadPath = "./uploads";
}

void Profiler::parseCommandLine(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--profile" || arg == "-p") {
      if (i + 1 < argc) {
        config_.name = argv[++i];
        config_.isTest = (config_.name == "test");
      }
    } else if (arg == "--test" || arg == "-t") {
      config_.name = "test";
      config_.isTest = true;
      config_.playerPort = 9093;
    } else if (arg == "--production" || arg == "--prod" ||
               arg == "production") {
      config_.name = "production";
      config_.isTest = false;
      config_.playerPort = 8083;
    } else if (arg == "--port" && i + 1 < argc) {
      config_.port = std::stoi(argv[++i]);
    } else if (arg == "--player-port" && i + 1 < argc) {
      config_.playerPort = std::stoi(argv[++i]);
    } else if (arg == "--address" && i + 1 < argc) {
      config_.address = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      printHelp(argv[0]);
      exit(0);
    }
  }
}

void Profiler::printHelp(const char *programName) const {
  std::cout << "Usage: " << programName << " [OPTIONS]\n"
            << "Options:\n"
            << "  -p, --profile PROFILE  Use profile (test/production)\n"
            << "  -t, --test            Test mode\n"
            << "  --production, --prod  Production mode\n"
            << "  --port PORT           Override web port\n"
            << "  --player-port PORT    Override player port\n"
            << "  --address ADDR        Override address\n"
            << "  --help, -h            Show help\n";
}

void Profiler::loadConfigurationFromFile() {
  fs::path configPath = findConfigFile();
  if (configPath.empty()) {
    std::cout << "No configuration file found, using defaults" << std::endl;
    return;
  }
  std::cout << "Loading config from: \"" << configPath << "\"" << std::endl;
  std::ifstream file(configPath);
  if (!file.is_open()) {
    std::cerr << "Warning: Could not open config file: " << configPath
              << std::endl;
    return;
  }
  try {
    nlohmann::json fullConfig = nlohmann::json::parse(file);
    parseConfigJson(fullConfig);
  } catch (const std::exception &e) {
    std::cerr << "Error parsing config: " << e.what() << std::endl;
  }
}

bool Profiler::loadConfigFromFile(const fs::path &configPath) {
  std::ifstream file(configPath);
  if (!file.is_open())
    return false;
  try {
    nlohmann::json fullConfig = nlohmann::json::parse(file);
    parseConfigJson(fullConfig);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error parsing config: " << e.what() << std::endl;
    return false;
  }
}

void Profiler::parseConfigJson(const nlohmann::json &fullConfig) {
  if (fullConfig.contains("profiles") &&
      fullConfig["profiles"].contains(config_.name)) {
    drogonConfig_ = fullConfig["profiles"][config_.name];
    std::cout << "Loaded profile: " << config_.name << std::endl;
  } else {
    drogonConfig_ = fullConfig;
    std::cout << "Using root config (no profile section)" << std::endl;
  }
  extractConfigValues();
}

void Profiler::extractConfigValues() {
  if (drogonConfig_.contains("player_port")) {
    config_.playerPort = drogonConfig_["player_port"].get<int>();
  }
  if (drogonConfig_.contains("app") &&
      drogonConfig_["app"].contains("document_root")) {
    config_.htmlPath = drogonConfig_["app"]["document_root"].get<std::string>();
    config_.documentRoot = config_.htmlPath;
  }
  if (drogonConfig_.contains("app")) {
    auto &app = drogonConfig_["app"];
    config_.threads = app.value("number_of_threads", config_.isTest ? 2 : 8);
    if (app.contains("log")) {
      auto &logConfig = app["log"];
      config_.logLevel =
          logConfig.value("log_level", config_.isTest ? "DEBUG" : "INFO");
      config_.logPath = logConfig.value(
          "log_path", config_.isTest ? "./logs" : "/var/log/media-explorer");
    }
    config_.uploadPath = app.value(
        "upload_path",
        config_.isTest ? "./uploads" : "/var/lib/media-explorer/uploads");
  }
  if (drogonConfig_.contains("listeners") &&
      !drogonConfig_["listeners"].empty()) {
    auto &listener = drogonConfig_["listeners"][0];
    config_.address = listener.value("address", config_.address);
    config_.port = listener.value("port", config_.port);
  }
  if (drogonConfig_.contains("app") &&
      drogonConfig_["app"].contains("database_path")) {
    config_.databasePath =
        drogonConfig_["app"]["database_path"].get<std::string>();
  }
  if (drogonConfig_.contains("app") &&
      drogonConfig_["app"].contains("music_directory")) {
    config_.musicDirectory =
        drogonConfig_["app"]["music_directory"].get<std::string>();
  }
  validateDocumentRoot();
}

void Profiler::validateDocumentRoot() {
  if (config_.htmlPath.empty()) {
    std::cerr << "ERROR: 'document_root' not found in config.json for profile '"
              << config_.name << "'" << std::endl;
    throw std::runtime_error(
        "Missing required 'document_root' in configuration");
  }
  if (!fs::exists(config_.htmlPath)) {
    std::cerr << "ERROR: HTML path does not exist: " << config_.htmlPath
              << std::endl;
    throw std::runtime_error("HTML path does not exist: " + config_.htmlPath);
  }
}

void Profiler::applyConfigDefaults() {
  if (!drogonConfig_.empty())
    return;
  drogonConfig_["app"]["number_of_threads"] = config_.isTest ? 2 : 8;
  drogonConfig_["app"]["log"]["log_level"] = config_.isTest ? "DEBUG" : "INFO";
  drogonConfig_["app"]["log"]["log_path"] =
      config_.isTest ? "./logs" : "/var/log/media-explorer";
  drogonConfig_["app"]["upload_path"] =
      config_.isTest ? "./uploads" : "/var/lib/media-explorer/uploads";
  drogonConfig_["listeners"] = nlohmann::json::array();
  drogonConfig_["listeners"].push_back(
      {{"address", config_.address}, {"port", config_.port}, {"https", false}});
  drogonConfig_["player_port"] = config_.playerPort;
}

void Profiler::findIndexFile() {
  std::vector<fs::path> searchPaths;
  if (!config_.documentRoot.empty()) {
    searchPaths.push_back(fs::path(config_.documentRoot) / "index.html");
  }
  auto defaultPaths = getIndexSearchPaths();
  searchPaths.insert(searchPaths.end(), defaultPaths.begin(),
                     defaultPaths.end());
  fs::path foundPath;
  if (!findIndexFileInPaths(searchPaths, foundPath)) {
    logSearchPaths(searchPaths);
    throw std::runtime_error("Could not find index.html");
  }
  config_.indexPath = foundPath.string();
  validateIndexFile();
  std::cout << "Found index.html at: " << config_.indexPath << std::endl;
}

bool Profiler::findIndexFileInPaths(const std::vector<fs::path> &paths,
                                    fs::path &foundPath) {
  for (const auto &path : paths) {
    std::cout << "Checking: " << path << std::endl;
    if (fs::exists(path)) {
      foundPath = path;
      return true;
    }
  }
  return false;
}

void Profiler::validateIndexFile() {
  fs::path indexPath = fs::path(config_.htmlPath) / "index.html";
  if (!fs::exists(indexPath)) {
    throw std::runtime_error("index.html not found at: " + indexPath.string());
  }
  config_.indexPath = indexPath.string();
}

void Profiler::logSearchPaths(const std::vector<fs::path> &paths) const {
  std::cerr << "Error: Could not find index.html" << std::endl;
  std::cerr << "Searched in:" << std::endl;
  for (const auto &path : paths) {
    std::cerr << "  " << path << std::endl;
  }
}

void Profiler::setupDrogonConfig() {
  setupDocumentRoot();
  setupListeners();
  setupAppConfig();
}

void Profiler::setupDocumentRoot() {
  if (!drogonConfig_.contains("app")) {
    drogonConfig_["app"] = nlohmann::json::object();
  }
  auto &app = drogonConfig_["app"];
  if (!app.contains("document_root") && !config_.documentRoot.empty()) {
    app["document_root"] = config_.documentRoot;
  }
  app["document_root"] = config_.htmlPath;
}

void Profiler::setupListeners() {
  if (drogonConfig_.contains("listeners") &&
      !drogonConfig_["listeners"].empty()) {
    return;
  }
  drogonConfig_["listeners"] = nlohmann::json::array();
  drogonConfig_["listeners"].push_back(
      {{"address", config_.address}, {"port", config_.port}, {"https", false}});
}

void Profiler::setupAppConfig() {
  if (!drogonConfig_.contains("app")) {
    drogonConfig_["app"] = nlohmann::json::object();
  }
  auto &app = drogonConfig_["app"];
  if (app.contains("document_root")) {
    std::string docRoot = app["document_root"].get<std::string>();
    if (!docRoot.empty()) {
      config_.documentRoot = docRoot;
      std::cout << "Configured document root: " << docRoot << std::endl;
    }
  }
}

fs::path Profiler::findConfigFile() const {
  const char *envPath = getenv("CONFIG_PATH");
  if (envPath && fs::exists(envPath)) {
    return envPath;
  }
  for (const auto &path : getConfigSearchPaths()) {
    if (fs::exists(path)) {
      return path;
    }
  }
  return fs::path();
}

std::vector<fs::path> Profiler::getConfigSearchPaths() const {
  std::vector<fs::path> paths;
  paths.push_back(fs::current_path() / "config.json");
  paths.push_back("/usr/local/etc/media-explorer-drogon/config.json");
  paths.push_back("/etc/media-explorer-drogon/config.json");
  if (const char *home = getenv("HOME")) {
    paths.push_back(fs::path(home) /
                    ".config/media-explorer-drogon/config.json");
  }
  return paths;
}

std::vector<fs::path> Profiler::getIndexSearchPaths() const {
  std::vector<fs::path> paths;
  paths.push_back(fs::current_path() / "index.html");
  paths.push_back(fs::current_path() / "views" / "index.html");
  paths.push_back(fs::current_path() / "static" / "index.html");
  paths.push_back(fs::current_path() / "www" / "index.html");
  char exePath[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len != -1) {
    exePath[len] = '\0';
    fs::path exeDir = fs::path(exePath).parent_path();
    paths.push_back(exeDir / "index.html");
    paths.push_back(exeDir / "views" / "index.html");
    paths.push_back(exeDir.parent_path() / "share" / "media-explorer-drogon" /
                    "views" / "index.html");
  }
  paths.push_back("/usr/local/share/media-explorer-drogon/views/index.html");
  paths.push_back("/usr/share/media-explorer-drogon/views/index.html");
  paths.push_back("/usr/local/web/media-explorer/index.html");
  if (const char *home = getenv("HOME")) {
    paths.push_back(fs::path(home) /
                    ".local/share/media-explorer-drogon/views/index.html");
    paths.push_back(fs::path(home) / "media-explorer" / "views" / "index.html");
  }
  return paths;
}

size_t Profiler::parseBodySize(const std::string &sizeStr) const {
  size_t size = 16 * 1024 * 1024;
  size_t pos = sizeStr.find('M');
  if (pos != std::string::npos) {
    size = std::stoul(sizeStr.substr(0, pos)) * 1024 * 1024;
  }
  return size;
}

void Profiler::parseHeaderString(
    const std::string &headerStr,
    std::vector<std::pair<std::string, std::string>> &headers) const {
  size_t colonPos = headerStr.find(':');
  if (colonPos == std::string::npos)
    return;
  std::string key = headerStr.substr(0, colonPos);
  std::string value = headerStr.substr(colonPos + 1);
  while (!value.empty() && value[0] == ' ') {
    value.erase(0, 1);
  }
  headers.push_back({key, value});
}

void Profiler::printStartupInfo() const {
  std::cout << "==========================================" << std::endl;
  std::cout << "Media Explorer Web Server (Drogon)" << std::endl;
  std::cout << "Version: 1.0.0" << std::endl;
  std::cout << "Profile: " << config_.name;
  if (config_.isTest)
    std::cout << " (TEST MODE)";
  std::cout << std::endl;
  std::cout << "HTML Path: " << config_.htmlPath << std::endl;
  std::cout << "Web Port: " << config_.port << std::endl;
  std::cout << "Player Port: " << config_.playerPort << std::endl;
  std::cout << "Address: " << config_.address << std::endl;
  std::cout << "Document Root: " << config_.documentRoot << std::endl;
  std::cout << "Index File: " << config_.indexPath << std::endl;
  std::cout << "Upload Path: " << config_.uploadPath << std::endl;
  std::cout << "Log Path: " << config_.logPath << std::endl;
  std::cout << "Log Level: " << config_.logLevel << std::endl;
  std::cout << "Threads: " << config_.threads << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "Web interface: http://" << config_.address << ":"
            << config_.port << "/" << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;
}
