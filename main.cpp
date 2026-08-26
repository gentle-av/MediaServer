#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicScanController.h"
#include "controllers/playlists/PlaylistController.h"
#include "database/MusicDatabase.h"
#include "database/PlaylistDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include "services/music/MetadataCache.h"
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
    auto musicDb = std::make_unique<MusicDatabase>(config.databasePath);
    if (!musicDb->init()) {
      std::cerr << "Failed to initialize music database" << std::endl;
      return 1;
    }
    auto playlistDb = std::make_unique<PlaylistDatabase>(config.databasePath);
    if (!playlistDb->init()) {
      std::cerr << "Failed to initialize playlist database" << std::endl;
      return 1;
    }
    auto musicRepo = std::make_unique<MusicRepository>(std::move(musicDb));
    auto playlistRepo = std::make_unique<PlaylistRepository>(
        std::move(playlistDb), std::move(musicRepo));
    WebAppConfig appConfig;
    appConfig.port = config.port;
    appConfig.documentRoot = config.documentRoot;
    appConfig.staticDir = config.documentRoot + "/static";
    appConfig.templateDir = config.documentRoot + "/templates";
    appConfig.indexFile = "index.html";
    appConfig.enableCache = true;
    appConfig.charset = "utf-8";
    App app(appConfig);
    auto cache = std::make_shared<MetadataCache>(500);
    MusicLibraryController libraryController(app, *musicRepo, cache);
    libraryController.register_routes();
    MusicScanController scanController(app, *musicRepo, *playlistRepo,
                                       config.musicDirectory);
    scanController.register_routes();
    PlaylistController playlistController(app, *playlistRepo, *musicRepo);
    playlistController.register_routes();
    if (!app.start()) {
      std::cerr << "Failed to start server: " << app.getLastError()
                << std::endl;
      return 1;
    }
    profiler.printStartupInfo();
    std::cout << "\nServer running on http://localhost:" << config.port
              << std::endl;
    std::cout << "Press Ctrl+C to stop the server\n" << std::endl;
    app.printEndpoints();
    while (running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    app.stop();
    std::cout << "Server stopped successfully" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
