#include "profilers/Profiler.h"
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    Profiler profiler(argc, argv);

    auto config = profiler.getConfig();
    std::cout << "\n=== Profiler Configuration ===" << std::endl;
    std::cout << "Profile: " << config.name << std::endl;
    std::cout << "Mode: " << (config.isTest ? "TEST" : "PRODUCTION")
              << std::endl;
    std::cout << "Web Port: " << config.port << std::endl;
    std::cout << "Player Port: " << config.playerPort << std::endl;
    std::cout << "Address: " << config.address << std::endl;
    std::cout << "HTML Path: " << config.htmlPath << std::endl;
    std::cout << "Document Root: " << config.documentRoot << std::endl;
    std::cout << "Index Path: " << config.indexPath << std::endl;
    std::cout << "Upload Path: " << config.uploadPath << std::endl;
    std::cout << "Log Path: " << config.logPath << std::endl;
    std::cout << "Log Level: " << config.logLevel << std::endl;
    std::cout << "Threads: " << config.threads << std::endl;
    std::cout << "Database Path: " << config.databasePath << std::endl;
    std::cout << "Music Directory: " << config.musicDirectory << std::endl;

    std::cout << "\n=== Drogon Config ===" << std::endl;
    auto drogonConfig = profiler.getDrogonConfig();
    std::cout << drogonConfig.dump(2) << std::endl;

    std::cout << "\n=== Startup Info ===" << std::endl;
    profiler.printStartupInfo();

    std::cout << "\n=== Testing Getters ===" << std::endl;
    std::cout << "getIndexPath(): " << profiler.getIndexPath() << std::endl;
    std::cout << "getDocumentRoot(): " << profiler.getDocumentRoot()
              << std::endl;
    std::cout << "getHtmlPath(): " << profiler.getHtmlPath() << std::endl;
    std::cout << "getDatabasePath(): " << profiler.getDatabasePath()
              << std::endl;
    std::cout << "getPlayerPort(): " << profiler.getPlayerPort() << std::endl;

    std::cout << "\n=== Profiler initialized successfully ===" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
