#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicScanController.h"
#include "controllers/player/PlayerController.h"
#include "controllers/playlists/PlaylistController.h"
#include "controllers/video/VideoController.h"
#include "database/ImageDatabase.h"
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

    if (!std::filesystem::is_directory(config.databasePath))
      std::filesystem::create_directory(config.databasePath);

    auto musicDb =
        std::make_unique<MusicDatabase>(config.databasePath + "/music.db");
    if (!musicDb->init()) {
      std::cerr << "Failed to initialize music database" << std::endl;
      return 1;
    }

    auto playlistDb = std::make_unique<PlaylistDatabase>(config.databasePath +
                                                         "/playlists.db");
    if (!playlistDb->init()) {
      std::cerr << "Failed to initialize playlist database" << std::endl;
      return 1;
    }

    auto imageDb =
        std::make_unique<ImageDatabase>(config.databasePath + "/thumbnails.db");
    if (!imageDb->init()) {
      std::cerr << "Failed to initialize image database" << std::endl;
      return 1;
    }

    auto musicRepo = std::make_shared<MusicRepository>(std::move(musicDb));
    auto playlistRepo =
        std::make_shared<PlaylistRepository>(std::move(playlistDb), musicRepo);

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

    PlayerController playerController(app, *musicRepo, *playlistRepo, cache);
    playerController.register_routes();

    VideoController videoController(app, std::make_shared<Profiler>(profiler));
    videoController.register_routes();

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
