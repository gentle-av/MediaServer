#include "controllers/VideoController.h"
#include <atomic>
#include <csignal>
#include <drogon/drogon.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
std::atomic<bool> running{true};

void signalHandler(int signum) {
  std::cout << "\nInterrupt signal (" << signum << ") received.\n";
  running = false;
  drogon::app().quit();
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  try {
    // drogon::app().registerController(std::make_shared<VideoController>());

    std::vector<fs::path> searchPaths;
    searchPaths.push_back("/usr/local/web/media-explorer/index.html");
    searchPaths.push_back(fs::current_path() / "views" / "index.html");
    fs::path exePath = fs::path(argv[0]).parent_path();
    searchPaths.push_back(exePath / "views" / "index.html");
    searchPaths.push_back(exePath.parent_path() / "share" /
                          "media-explorer-drogon" / "views" / "index.html");
    searchPaths.push_back(
        "/usr/local/share/media-explorer-drogon/views/index.html");
    searchPaths.push_back("/usr/share/media-explorer-drogon/views/index.html");
    const char *home = getenv("HOME");
    if (home) {
      searchPaths.push_back(
          fs::path(home) /
          ".local/share/media-explorer-drogon/views/index.html");
    }

    fs::path indexPath;
    for (const auto &path : searchPaths) {
      std::cout << "Looking for index.html at: " << path << std::endl;
      if (fs::exists(path)) {
        indexPath = path;
        break;
      }
    }

    if (indexPath.empty()) {
      std::cerr << "Error: Could not find index.html" << std::endl;
      return 1;
    }

    std::cout << "Found index.html at: " << indexPath << std::endl;

    const char *configPath = getenv("CONFIG_PATH");
    if (configPath && fs::exists(configPath)) {
      std::cout << "Loading config from: " << configPath << std::endl;
      drogon::app().loadConfigFile(configPath);
    } else {
      std::vector<fs::path> configPaths = {
          "/usr/local/etc/media-explorer-drogon/config.json",
          "/etc/media-explorer-drogon/config.json", "./config.json"};
      for (const auto &path : configPaths) {
        if (fs::exists(path)) {
          std::cout << "Loading config from: " << path << std::endl;
          drogon::app().loadConfigFile(path.string());
          break;
        }
      }
    }

    // Настраиваем документ рут для статических файлов
    drogon::app().setDocumentRoot(
        "/home/avr/code/projects/cpp/build/MediaServer/views");

    std::cout << "==========================================" << std::endl;
    std::cout << "Media Explorer Web Server (Drogon)" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Server starting on: http://localhost:8083" << std::endl;
    std::cout << "Web interface: http://localhost:8083/" << std::endl;
    std::cout << "API endpoint: http://localhost:8083/api/list" << std::endl;
    std::cout << "Serving directory: /mnt/video" << std::endl;
    std::cout << "Index file: " << indexPath << std::endl;
    std::cout
        << "Document root: /home/avr/code/projects/cpp/build/MediaServer/views"
        << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    drogon::app().addListener("0.0.0.0", 8083).run();

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
