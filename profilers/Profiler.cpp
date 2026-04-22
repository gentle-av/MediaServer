#include "profilers/Profiler.h"
#include <fstream>
#include <iostream>
#include <limits.h>
#include <unistd.h>

Profiler::Profiler(int argc, char *argv[]) {
  parseCommandLine(argc, argv);
  loadConfiguration();
  findIndexFile();
  setupDrogonConfig();
}

void Profiler::parseCommandLine(int argc, char *argv[]) {
  config_.name = "test";
  config_.isTest = true;
  config_.port = 8083;
  config_.playerPort = 9093;
  config_.address = "127.0.0.1";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--profile" || arg == "-p") && i + 1 < argc) {
      config_.name = argv[++i];
      config_.isTest = (config_.name == "test");
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
      std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                << "Options:\n"
                << "  -p, --profile PROFILE  Use profile (test/production)\n"
                << "  -t, --test            Test mode\n"
                << "  --production, --prod  Production mode\n"
                << "  --port PORT           Override web port\n"
                << "  --player-port PORT    Override player port\n"
                << "  --address ADDR        Override address\n"
                << "  --help, -h            Show help\n";
      exit(0);
    }
  }
}

void Profiler::loadConfiguration() {
  fs::path configPath = findConfigFile();
  if (!configPath.empty()) {
    std::cout << "Loading config from: \"" << configPath << "\"" << std::endl;
    std::ifstream file(configPath);
    if (file.is_open()) {
      try {
        nlohmann::json fullConfig = nlohmann::json::parse(file);
        if (fullConfig.contains("profiles") &&
            fullConfig["profiles"].contains(config_.name)) {
          drogonConfig_ = fullConfig["profiles"][config_.name];
          if (drogonConfig_.contains("player_port")) {
            config_.playerPort = drogonConfig_["player_port"].get<int>();
          }
          std::cout << "Loaded profile: " << config_.name << std::endl;
        } else {
          drogonConfig_ = fullConfig;
          if (drogonConfig_.contains("player_port")) {
            config_.playerPort = drogonConfig_["player_port"].get<int>();
          }
          std::cout << "Using root config (no profile section)" << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
      }
    }
  }
  if (drogonConfig_.contains("app") &&
      drogonConfig_["app"].contains("document_root")) {
    config_.htmlPath = drogonConfig_["app"]["document_root"].get<std::string>();
  }
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
  config_.documentRoot = config_.htmlPath;
  if (drogonConfig_.empty()) {
    drogonConfig_["app"]["number_of_threads"] = config_.isTest ? 2 : 8;
    drogonConfig_["app"]["log"]["log_level"] =
        config_.isTest ? "DEBUG" : "INFO";
    drogonConfig_["app"]["log"]["log_path"] =
        config_.isTest ? "./logs" : "/var/log/media-explorer";
    drogonConfig_["app"]["upload_path"] =
        config_.isTest ? "./uploads" : "/var/lib/media-explorer/uploads";
    drogonConfig_["listeners"] = nlohmann::json::array();
    drogonConfig_["listeners"].push_back({{"address", config_.address},
                                          {"port", config_.port},
                                          {"https", false}});
    drogonConfig_["player_port"] = config_.playerPort;
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
  for (const auto &path : searchPaths) {
    std::cout << "Checking: " << path << std::endl;
    if (fs::exists(path)) {
      foundPath = path;
      break;
    }
  }
  if (foundPath.empty()) {
    std::cerr << "Error: Could not find index.html" << std::endl;
    std::cerr << "Searched in:" << std::endl;
    for (const auto &path : searchPaths) {
      std::cerr << "  " << path << std::endl;
    }
    throw std::runtime_error("Could not find index.html");
  }
  config_.indexPath = foundPath.string();
  fs::path indexPath = fs::path(config_.htmlPath) / "index.html";
  if (!fs::exists(indexPath)) {
    throw std::runtime_error("index.html not found at: " + indexPath.string());
  }
  config_.indexPath = indexPath.string();
  std::cout << "Found index.html at: " << config_.indexPath << std::endl;
}

void Profiler::setupDrogonConfig() {
  if (!drogonConfig_.contains("app")) {
    drogonConfig_["app"] = nlohmann::json::object();
  }
  auto &app = drogonConfig_["app"];
  if (!app.contains("document_root") && !config_.documentRoot.empty()) {
    app["document_root"] = config_.documentRoot;
  }
  if (!drogonConfig_.contains("listeners") ||
      drogonConfig_["listeners"].empty()) {
    drogonConfig_["listeners"] = nlohmann::json::array();
    drogonConfig_["listeners"].push_back({{"address", config_.address},
                                          {"port", config_.port},
                                          {"https", false}});
  }
  if (app.contains("document_root")) {
    std::string docRoot = app["document_root"].get<std::string>();
    if (!docRoot.empty()) {
      config_.documentRoot = docRoot;
      std::cout << "Configured document root: " << docRoot << std::endl;
    }
  }
  app["document_root"] = config_.htmlPath;
}

fs::path Profiler::findConfigFile() const {
  auto searchPaths = getConfigSearchPaths();
  const char *envPath = getenv("CONFIG_PATH");
  if (envPath && fs::exists(envPath)) {
    return envPath;
  }
  for (const auto &path : searchPaths) {
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
  paths.push_back(fs::path(getenv("HOME") ? getenv("HOME") : "") /
                  ".config/media-explorer-drogon/config.json");
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
  const char *home = getenv("HOME");
  if (home) {
    paths.push_back(fs::path(home) /
                    ".local/share/media-explorer-drogon/views/index.html");
    paths.push_back(fs::path(home) / "media-explorer" / "views" / "index.html");
  }
  return paths;
}

void Profiler::applyToDrogon(drogon::HttpAppFramework &app) const {
  if (drogonConfig_.contains("app")) {
    auto &appConfig = drogonConfig_["app"];
    if (appConfig.contains("number_of_threads")) {
      app.setThreadNum(appConfig["number_of_threads"].get<int>());
    }
    if (appConfig.contains("client_max_body_size")) {
      std::string sizeStr =
          appConfig["client_max_body_size"].get<std::string>();
      size_t size = 16 * 1024 * 1024;
      if (sizeStr.find("M") != std::string::npos) {
        size = std::stoul(sizeStr.substr(0, sizeStr.find("M"))) * 1024 * 1024;
      }
      app.setClientMaxBodySize(size);
    }
    if (appConfig.contains("upload_path")) {
      app.setUploadPath(appConfig["upload_path"].get<std::string>());
    }
    if (appConfig.contains("log")) {
      auto &logConfig = appConfig["log"];
      if (logConfig.contains("log_path")) {
        app.setLogPath(logConfig["log_path"].get<std::string>());
      }
      if (logConfig.contains("log_level")) {
        app.setLogLevel(
            stringToLogLevel(logConfig["log_level"].get<std::string>()));
      }
    }
    std::string docRoot;
    if (appConfig.contains("document_root")) {
      docRoot = appConfig["document_root"].get<std::string>();
    }
    if (!docRoot.empty()) {
      if (fs::exists(docRoot)) {
        app.setDocumentRoot(docRoot);
        std::cout << "Document root set to: " << docRoot << std::endl;
      } else {
        std::cerr << "Warning: Document root does not exist: " << docRoot
                  << std::endl;
        fs::create_directories(docRoot);
        app.setDocumentRoot(docRoot);
        std::cout << "Created and set document root to: " << docRoot
                  << std::endl;
      }
    } else {
      std::cerr << "Warning: document_root not found in config" << std::endl;
      if (fs::exists("./views")) {
        app.setDocumentRoot("./views");
        std::cout << "Using fallback: ./views" << std::endl;
      } else if (fs::exists(fs::current_path() / "views")) {
        app.setDocumentRoot((fs::current_path() / "views").string());
        std::cout << "Using fallback: "
                  << (fs::current_path() / "views").string() << std::endl;
      } else {
        fs::create_directories("./views");
        app.setDocumentRoot("./views");
        std::cout << "Created and using: ./views" << std::endl;
      }
    }
    if (appConfig.contains("static_file_headers")) {
      auto &headers = appConfig["static_file_headers"];
      std::vector<std::pair<std::string, std::string>> headerList;
      for (auto &header : headers) {
        if (header.contains("headers") && header["headers"].is_array()) {
          for (auto &h : header["headers"]) {
            std::string headerStr = h.get<std::string>();
            size_t colonPos = headerStr.find(':');
            if (colonPos != std::string::npos) {
              std::string key = headerStr.substr(0, colonPos);
              std::string value = headerStr.substr(colonPos + 1);
              while (!value.empty() && value[0] == ' ')
                value.erase(0, 1);
              headerList.push_back({key, value});
            }
          }
        }
      }
      if (!headerList.empty()) {
        app.setStaticFileHeaders(headerList);
        std::cout << "Set static file headers: " << headerList.size()
                  << " headers" << std::endl;
      }
    }
  }
  if (drogonConfig_.contains("listeners") &&
      drogonConfig_["listeners"].is_array()) {
    for (auto &listener : drogonConfig_["listeners"]) {
      std::string address = listener.value("address", config_.address);
      int port = listener.value("port", config_.port);
      bool https = listener.value("https", false);
      app.addListener(address, port, https);
      std::cout << "Listening on " << address << ":" << port << std::endl;
    }
  }
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

trantor::Logger::LogLevel
Profiler::stringToLogLevel(const std::string &level) const {
  if (level == "TRACE")
    return trantor::Logger::kTrace;
  if (level == "DEBUG")
    return trantor::Logger::kDebug;
  if (level == "INFO")
    return trantor::Logger::kInfo;
  if (level == "WARN")
    return trantor::Logger::kWarn;
  if (level == "ERROR")
    return trantor::Logger::kError;
  return trantor::Logger::kInfo;
}
