#include "core/MediaServerCore.h"
#include "controllers/albums/AlbumArtController.h"
#include "controllers/albums/AlbumManagementController.h"
#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicMetadataController.h"
#include "controllers/music/MusicPlaybackController.h"
#include "controllers/music/MusicScanController.h"
#include "controllers/player/PlayerController.h"
#include "controllers/playlists/PlaylistController.h"
#include "controllers/power/PowerController.h"
#include "controllers/video/VideoController.h"
#include "database/MusicDatabase.h"
#include "database/PlaylistDatabase.h"
#include "middlewares/CorsMiddleware.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include "services/music/MetadataCache.h"
#include "services/video/FileSystemService.h"
#include <filesystem>
#include <future>
#include <iostream>

MediaServerCore::MediaServerCore(int argc, char *argv[])
    : profiler(std::make_unique<Profiler>(argc, argv)) {
  config = profiler->getConfig();
}

MediaServerCore::~MediaServerCore() { shutdown(); }

bool MediaServerCore::run() {
  if (!initialize()) {
    return false;
  }
  if (!startServer()) {
    return false;
  }
  profiler->printStartupInfo();
  std::cout << "\nMedia Server running on http://localhost:" << config.port
            << std::endl;
  std::cout << "Press Ctrl+C to stop the server\n" << std::endl;
  app->printEndpoints();
  runMainLoop();
  return true;
}

void MediaServerCore::shutdown() {
  running = false;
  if (mainLoopThread.joinable()) {
    mainLoopThread.request_stop();
    mainLoopThread.join();
  }
  if (app) {
    app->stop();
  }
  std::cout << "Media Server stopped successfully" << std::endl;
}

bool MediaServerCore::initialize() {
  if (!std::filesystem::is_directory(config.databasePath)) {
    std::filesystem::create_directory(config.databasePath);
  }
  if (!initializeDatabases()) {
    std::cerr << "Failed to initialize databases" << std::endl;
    return false;
  }
  if (!initializeRepositories())
    return false;
  if (!initializeServices())
    return false;
  if (!initializeServer())
    return false;
  if (!initializeCors())
    return false;
  if (!initializeControllers())
    return false;
  return true;
}

bool MediaServerCore::initializeCors() {
  if (!app) {
    std::cerr << "App not initialized" << std::endl;
    return false;
  }
  std::cout << "✅ CORS middleware enabled" << std::endl;
  app->useMiddleware([](StringHttpRequest &req,
                        StringHttpResponse &resp) -> bool {
    return CorsMiddleware<StringHttpRequest, StringHttpResponse>::process(req,
                                                                          resp);
  });
  return true;
}

bool MediaServerCore::initializeDatabases() {
  musicDb = std::make_shared<MusicDatabase>(config.databasePath + "/music.db");
  if (!musicDb->init()) {
    std::cerr << "Failed to initialize music database" << std::endl;
    return false;
  }
  playlistDb =
      std::make_shared<PlaylistDatabase>(config.databasePath + "/playlists.db");
  if (!playlistDb->init()) {
    std::cerr << "Failed to initialize playlist database" << std::endl;
    return false;
  }
  return true;
}

bool MediaServerCore::initializeRepositories() {
  musicRepo = std::make_shared<MusicRepository>(musicDb);
  playlistRepo = std::make_shared<PlaylistRepository>(playlistDb, musicRepo);
  return true;
}

bool MediaServerCore::initializeServices() {
  cache = std::make_shared<MetadataCache>(500);
  auto &fsService = FileSystemService::getInstance();
  std::vector<std::string> allowedPaths;
  allowedPaths.push_back(config.videoDirectory);
  allowedPaths.push_back("/mnt/media");
  fsService.setAllowedPaths(allowedPaths);
  return true;
}

bool MediaServerCore::initializeServer() {
  WebAppConfig appConfig;
  appConfig.port = config.port;
  appConfig.documentRoot = config.documentRoot;
  appConfig.staticDir = config.documentRoot + "/static";
  appConfig.templateDir = config.documentRoot + "/templates";
  appConfig.indexFile = "index.html";
  appConfig.enableCache = true;
  appConfig.charset = "utf-8";
  app = std::make_unique<App>(appConfig);
  return true;
}

bool MediaServerCore::initializeControllers() {
  libraryController =
      std::make_unique<MusicLibraryController>(*app, *musicRepo, cache);
  libraryController->register_routes();
  scanController = std::make_unique<MusicScanController>(
      *app, *musicRepo, *playlistRepo, config.musicDirectory);
  scanController->register_routes();
  playlistController =
      std::make_unique<PlaylistController>(*app, *playlistRepo, *musicRepo);
  playlistController->register_routes();
  playerController = std::make_shared<PlayerController>(*app, *musicRepo,
                                                        *playlistRepo, cache);
  playerController->register_routes();
  metadataController =
      std::make_unique<MusicMetadataController>(*app, musicDb, cache);
  metadataController->register_routes();
  playbackController = std::make_unique<MusicPlaybackController>(
      *app, musicDb, playerController);
  playbackController->register_routes();
  videoController = std::make_unique<VideoController>(
      *app, std::make_shared<Profiler>(*profiler));
  videoController->register_routes();
  powerController = std::make_unique<PowerController>(*app);
  powerController->register_routes();
  albumArtController =
      std::make_unique<AlbumArtController>(*app, musicDb, *musicRepo);
  albumArtController->register_routes();
  albumManagementController = std::make_unique<AlbumManagementController>(
      *app, musicDb, cache, *musicRepo);
  albumManagementController->register_routes();
  return true;
}

bool MediaServerCore::startServer() {
  if (!app->start()) {
    std::cerr << "Failed to start server: " << app->getLastError() << std::endl;
    return false;
  }
  return true;
}

void MediaServerCore::runMainLoop() {
  mainLoopThread = std::jthread([this](std::stop_token stopToken) {
    while (!stopToken.stop_requested() && running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
}
