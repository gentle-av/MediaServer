#include "controllers/music/MusicScanController.h"
#include "database/MusicDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include <chrono>
#include <csignal>
#include <html-server/app/App.h>
#include <html-server/templates/WebApplication.h>
#include <iostream>
#include <memory>
#include <thread>

std::atomic<bool> running{true};

void signalHandler(int) { running = false; }

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  try {
    Profiler profiler(argc, argv);
    auto config = profiler.getConfig();

    auto db = std::make_unique<MusicDatabase>(config.databasePath);
    if (!db->init()) {
      std::cerr << "Failed to initialize database" << std::endl;
      return 1;
    }
    MusicRepository repository(std::move(db));

    App::Config appConfig;
    appConfig.port = config.port;
    appConfig.documentRoot = config.documentRoot;
    appConfig.staticDir = config.documentRoot + "/static";
    appConfig.templateDir = config.documentRoot + "/templates";
    appConfig.indexFile = "index.html";
    appConfig.enableCache = true;
    appConfig.charset = "utf-8";

    App app(appConfig);
    MusicScanController controller(app, repository, config.musicDirectory);
    controller.register_routes();

    if (!app.start()) {
      std::cerr << "Failed to start server: " << app.getLastError()
                << std::endl;
      return 1;
    }

    profiler.printStartupInfo();
    std::cout << "Available endpoints:" << std::endl;
    std::cout << "  GET  /api/music/force-rescan - Force rescan music directory"
              << std::endl;
    std::cout << "  POST /api/music/remove-missing - Remove missing tracks"
              << std::endl;

    while (running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    app.stop();
    std::cout << "Server stopped" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
