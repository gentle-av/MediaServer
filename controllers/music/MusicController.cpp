#include "controllers/music/MusicController.h"
#include "controllers/albums/AlbumArtController.h"
#include "controllers/albums/AlbumManagementController.h"
#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicMetadataController.h"
#include "controllers/music/MusicPlaybackController.h"
#include "controllers/music/MusicScanController.h"
#include <drogon/drogon.h>
#include <filesystem>
#include <iostream>
#include <memory>

namespace fs = std::filesystem;

std::shared_ptr<MusicDatabase> MusicController::db_ = nullptr;
std::shared_ptr<MetadataCache> MusicController::cache_ = nullptr;
std::shared_ptr<MusicScanner> MusicController::scanner_ = nullptr;
std::shared_ptr<MusicLibraryController> MusicController::libraryController_ =
    nullptr;
std::shared_ptr<MusicMetadataController> MusicController::metadataController_ =
    nullptr;
std::shared_ptr<AlbumArtController> MusicController::albumArtController_ =
    nullptr;
std::shared_ptr<AlbumManagementController>
    MusicController::albumManagementController_ = nullptr;
std::shared_ptr<MusicScanController> MusicController::scanController_ = nullptr;

MusicController::MusicController() {
  std::cout << "[MusicController] Initializing..." << std::endl;
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  std::cout << "[MusicController] DB Path: " << dbPath << std::endl;
  fs::path dbDir = fs::path(dbPath).parent_path();
  if (!fs::exists(dbDir)) {
    std::cout << "[MusicController] Creating directory: " << dbDir << std::endl;
    fs::create_directories(dbDir);
  }
  db_ = std::make_shared<MusicDatabase>(dbPath);
  if (!db_->init()) {
    std::cerr << "[MusicController] Failed to initialize database!"
              << std::endl;
    return;
  }
  std::cout << "[MusicController] Database initialized" << std::endl;
  cache_ = std::make_shared<MetadataCache>();
  scanner_ = std::make_shared<MusicScanner>(*db_, *cache_, "/mnt/media/music");
  std::cout << "[MusicController] MusicScanner created" << std::endl;
  libraryController_ = std::make_shared<MusicLibraryController>();
  libraryController_->init(db_, cache_);
  std::cout << "[MusicController] MusicLibraryController created" << std::endl;
  metadataController_ = std::make_shared<MusicMetadataController>();
  metadataController_->init(db_, cache_);
  std::cout << "[MusicController] MusicMetadataController created" << std::endl;
  albumArtController_ = std::make_shared<AlbumArtController>();
  albumArtController_->init(db_);
  std::cout << "[MusicController] AlbumArtController created" << std::endl;
  albumManagementController_ = std::make_shared<AlbumManagementController>();
  albumManagementController_->init(db_, cache_);
  std::cout << "[MusicController] AlbumManagementController created"
            << std::endl;
  scanController_ = std::make_shared<MusicScanController>();
  scanController_->init(db_, cache_, scanner_);
  std::cout << "[MusicController] MusicScanController created and initialized"
            << std::endl;
}

void MusicController::init(std::shared_ptr<PlayerController> playerController) {
  MusicPlaybackController::init(db_, playerController);
}
