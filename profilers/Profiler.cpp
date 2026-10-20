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
}

void Profiler::initializeConfiguration() { setDefaultConfigValues(); }

void Profiler::setDefaultConfigValues() {
  config.name = "test";
  config.isTest = true;
  config.port = 8083;
  config.address = "127.0.0.1";
  config.threads = 8;
  config.logLevel = "DEBUG";
  config.logPath = "./logs";
  config.uploadPath = "./uploads";
  config.musicDirectory = "/mnt/media/music";
  config.databasePath = "./media.db";
  config.htmlPath = "./views";
  config.documentRoot = "./views";
  config.videoDirectory = "/mnt/video";
  config.booksDirectory = "/mnt/media/books";
}

void Profiler::parseCommandLine(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--profile" || arg == "-p") {
      if (i + 1 < argc) {
        config.name = argv[++i];
        config.isTest = (config.name == "test");
      }
    } else if (arg == "--test" || arg == "-t") {
      config.name = "test";
      config.isTest = true;
    } else if (arg == "--production" || arg == "--prod" ||
               arg == "production") {
      config.name = "production";
      config.isTest = false;
    } else if (arg == "--port" && i + 1 < argc) {
      config.port = std::stoi(argv[++i]);
    } else if (arg == "--address" && i + 1 < argc) {
      config.address = argv[++i];
    } else if (arg == "--music-dir" && i + 1 < argc) {
      config.musicDirectory = argv[++i];
    } else if (arg == "--db-path" && i + 1 < argc) {
      config.databasePath = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      printHelp(argv[0]);
      exit(0);
    } else if (arg == "--books-dir" && i + 1 < argc) {
      config.booksDirectory = argv[++i];
    }
  }
}

void Profiler::printHelp(const char *programName) const {
  std::cout << "Usage: " << programName << " [OPTIONS]\n"
            << "Options:\n"
            << " -p, --profile PROFILE Use profile (test/production)\n"
            << " -t, --test Test mode\n"
            << " --production, --prod Production mode\n"
            << " --port PORT Override web port\n"
            << " --address ADDR Override address\n"
            << " --music-dir DIR Override music directory\n"
            << " --db-path PATH Override database path\n"
            << " --help, -h Show help\n";
}

void Profiler::loadConfigurationFromFile() {
  fs::path configPath = findConfigFile();
  if (configPath.empty()) {
    std::cout << "No configuration file found, using defaults" << '\n';
    return;
  }
  std::cout << "Loading config from: \"" << configPath << "\"" << '\n';
  std::ifstream file(configPath);
  if (!file.is_open()) {
    std::cerr << "Warning: Could not open config file: " << configPath << '\n';
    return;
  }
  try {
    nlohmann::json fullConfig = nlohmann::json::parse(file);
    parseConfigJson(fullConfig);
  } catch (const std::exception &e) {
    std::cerr << "Error parsing config: " << e.what() << '\n';
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
    std::cerr << "Error parsing config: " << e.what() << '\n';
    return false;
  }
}

void Profiler::parseConfigJson(const nlohmann::json &fullConfig) {
  nlohmann::json profileConfig;
  if (fullConfig.contains("profiles") &&
      fullConfig["profiles"].contains(config.name)) {
    profileConfig = fullConfig["profiles"][config.name];
    std::cout << "Loaded profile: " << config.name << '\n';
  } else {
    profileConfig = fullConfig;
    std::cout << "Using root config (no profile section)" << '\n';
  }
  extractConfigValues(profileConfig);
}

void Profiler::extractConfigValues(const nlohmann::json &profileConfig) {
  if (profileConfig.contains("app")) {
    const auto &app = profileConfig["app"];
    if (app.contains("document_root")) {
      config.htmlPath = app["document_root"].get<std::string>();
      config.documentRoot = config.htmlPath;
    }
    config.threads = app.value("number_of_threads", 8);
    if (app.contains("log")) {
      const auto &logConfig = app["log"];
      config.logLevel =
          logConfig.value("log_level", config.isTest ? "DEBUG" : "INFO");
      config.logPath = logConfig.value(
          "log_path", config.isTest ? "./logs" : "/var/log/media-explorer");
    }
    config.uploadPath = app.value(
        "upload_path",
        config.isTest ? "./uploads" : "/var/lib/media-explorer/uploads");
    if (app.contains("database_path")) {
      config.databasePath = app["database_path"].get<std::string>();
    }
  }
  if (profileConfig.contains("content")) {
    const auto &content = profileConfig["content"];
    if (content.contains("music_directory")) {
      config.musicDirectory = content["music_directory"].get<std::string>();
    }
    if (content.contains("video_directory")) {
      config.videoDirectory = content["video_directory"].get<std::string>();
    }
    if (content.contains("book_directory")) {
      config.booksDirectory = content["book_directory"].get<std::string>();
    }
  }
  if (profileConfig.contains("listeners") &&
      !profileConfig["listeners"].empty()) {
    const auto &listener = profileConfig["listeners"][0];
    config.address = listener.value("address", config.address);
    config.port = listener.value("port", config.port);
  }
  validateDocumentRoot();
}

void Profiler::validateDocumentRoot() {
  if (config.htmlPath.empty()) {
    std::cerr << "ERROR: 'document_root' not found in config.json for profile '"
              << config.name << "'" << '\n';
    throw std::runtime_error(
        "Missing required 'document_root' in configuration");
  }
  if (!fs::exists(config.htmlPath)) {
    std::cerr << "ERROR: HTML path does not exist: " << config.htmlPath << '\n';
    throw std::runtime_error("HTML path does not exist: " + config.htmlPath);
  }
}

void Profiler::applyConfigDefaults() {
  if (!config.htmlPath.empty() && config.htmlPath != "./views") {
    return;
  }
  config.threads = 8;
  config.logLevel = config.isTest ? "DEBUG" : "INFO";
  config.logPath = config.isTest ? "./logs" : "/var/log/media-explorer";
  config.uploadPath =
      config.isTest ? "./uploads" : "/var/lib/media-explorer/uploads";
  config.documentRoot = config.htmlPath;
  config.videoDirectory = "/mnt/video";
  config.musicDirectory = "/mnt/media/music";
  config.booksDirectory = "/mnt/media/books";
}

void Profiler::findIndexFile() {
  std::vector<fs::path> searchPaths;
  if (!config.documentRoot.empty()) {
    searchPaths.push_back(fs::path(config.documentRoot) / "index.html");
  }
  auto defaultPaths = getIndexSearchPaths();
  searchPaths.insert(searchPaths.end(), defaultPaths.begin(),
                     defaultPaths.end());
  fs::path foundPath;
  if (!findIndexFileInPaths(searchPaths, foundPath)) {
    logSearchPaths(searchPaths);
    throw std::runtime_error("Could not find index.html");
  }
  config.indexPath = foundPath.string();
  validateIndexFile();
  std::cout << "Found index.html at: " << config.indexPath << '\n';
}

bool Profiler::findIndexFileInPaths(const std::vector<fs::path> &paths,
                                    fs::path &foundPath) {
  for (const auto &path : paths) {
    std::cout << "Checking: " << path << '\n';
    if (fs::exists(path)) {
      foundPath = path;
      return true;
    }
  }
  return false;
}

void Profiler::validateIndexFile() {
  fs::path indexPath = fs::path(config.htmlPath) / "index.html";
  if (!fs::exists(indexPath)) {
    throw std::runtime_error("index.html not found at: " + indexPath.string());
  }
  config.indexPath = indexPath.string();
}

void Profiler::logSearchPaths(const std::vector<fs::path> &paths) const {
  std::cerr << "Error: Could not find index.html" << '\n';
  std::cerr << "Searched in:" << '\n';
  for (const auto &path : paths) {
    std::cerr << " " << path << '\n';
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
  if (const char *home = getenv("HOME")) {
    paths.push_back(fs::path(home) / ".local/share/media-explorer/config.json");
  }
  paths.push_back(fs::current_path() / "config.json");
  paths.push_back("/usr/local/etc/media-explorer-drogon/config.json");
  paths.push_back("/etc/media-explorer-drogon/config.json");
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
  std::cout << "==========================================" << '\n';
  std::cout << "Media Explorer Web Server (Drogon)" << '\n';
  std::cout << "Version: 1.0.0" << '\n';
  std::cout << "Profile: " << config.name;
  if (config.isTest)
    std::cout << " (TEST MODE)";
  std::cout << '\n';
  std::cout << "HTML Path: " << config.htmlPath << '\n';
  std::cout << "Address: " << config.address << '\n';
  std::cout << "Document Root: " << config.documentRoot << '\n';
  std::cout << "Index File: " << config.indexPath << '\n';
  std::cout << "Upload Path: " << config.uploadPath << '\n';
  std::cout << "Log Path: " << config.logPath << '\n';
  std::cout << "Log Level: " << config.logLevel << '\n';
  std::cout << "Threads: " << config.threads << '\n';
  std::cout << "Database Path: " << config.databasePath << '\n';
  std::cout << "Music Directory: " << config.musicDirectory << '\n';
  std::cout << "Video Directory: " << config.videoDirectory << '\n';
  std::cout << "Books Directory: " << config.booksDirectory << '\n';
  std::cout << "==========================================" << '\n';
  std::cout << "Web interface: http:"
            << "/" << '\n';
  std::cout << "Press Ctrl+C to stop" << '\n';
}
