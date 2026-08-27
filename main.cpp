#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicScanController.h"
#include "controllers/player/PlayerController.h"
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
    std::cout << "[main] Starting..." << std::endl;
    Profiler profiler(argc, argv);
    auto config = profiler.getConfig();
    std::cout << "[main] Config loaded" << std::endl;

    std::cout << "[main] Creating MusicDatabase..." << std::endl;
    auto musicDb = std::make_unique<MusicDatabase>(config.databasePath);
    if (!musicDb->init()) {
      std::cerr << "Failed to initialize music database" << std::endl;
      return 1;
    }
    std::cout << "[main] MusicDatabase initialized" << std::endl;

    std::cout << "[main] Creating PlaylistDatabase..." << std::endl;
    auto playlistDb = std::make_unique<PlaylistDatabase>(config.databasePath);
    if (!playlistDb->init()) {
      std::cerr << "Failed to initialize playlist database" << std::endl;
      return 1;
    }
    std::cout << "[main] PlaylistDatabase initialized" << std::endl;

    std::cout << "[main] Creating MusicRepository..." << std::endl;
    auto musicRepo = std::make_shared<MusicRepository>(std::move(musicDb));
    std::cout << "[main] Creating PlaylistRepository..." << std::endl;
    auto playlistRepo =
        std::make_shared<PlaylistRepository>(std::move(playlistDb), musicRepo);
    std::cout << "[main] Repositories created" << std::endl;

    WebAppConfig appConfig;
    appConfig.port = config.port;
    appConfig.documentRoot = config.documentRoot;
    appConfig.staticDir = config.documentRoot + "/static";
    appConfig.templateDir = config.documentRoot + "/templates";
    appConfig.indexFile = "index.html";
    appConfig.enableCache = true;
    appConfig.charset = "utf-8";
    std::cout << "[main] AppConfig created" << std::endl;

    std::cout << "[main] Creating App..." << std::endl;
    App app(appConfig);
    std::cout << "[main] App created" << std::endl;

    std::cout << "[main] Creating MetadataCache..." << std::endl;
    auto cache = std::make_shared<MetadataCache>(500);
    std::cout << "[main] MetadataCache created" << std::endl;

    std::cout << "[main] Creating MusicLibraryController..." << std::endl;
    MusicLibraryController libraryController(app, *musicRepo, cache);
    libraryController.register_routes();
    std::cout << "[main] MusicLibraryController registered" << std::endl;

    std::cout << "[main] Creating MusicScanController..." << std::endl;
    MusicScanController scanController(app, *musicRepo, *playlistRepo,
                                       config.musicDirectory);
    scanController.register_routes();
    std::cout << "[main] MusicScanController registered" << std::endl;

    std::cout << "[main] Creating PlaylistController..." << std::endl;
    PlaylistController playlistController(app, *playlistRepo, *musicRepo);
    playlistController.register_routes();
    std::cout << "[main] PlaylistController registered" << std::endl;

    std::cout << "[main] Creating PlayerController..." << std::endl;
    PlayerController playerController(app, *musicRepo, *playlistRepo, cache);
    playerController.register_routes();
    std::cout << "[main] PlayerController registered" << std::endl;

    std::cout << "[main] Starting app..." << std::endl;
    if (!app.start()) {
      std::cerr << "Failed to start server: " << app.getLastError()
                << std::endl;
      return 1;
    }
    std::cout << "[main] App started" << std::endl;

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
