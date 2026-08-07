#include "controllers/music/MusicController.h"
#include "controllers/albums/AlbumArtController.h"
#include "controllers/albums/AlbumManagementController.h"
#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicMetadataController.h"
#include "controllers/music/MusicPlaybackController.h"
#include "controllers/music/MusicScanController.h"
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

std::shared_ptr<MusicDatabase> MusicController::db_ = nullptr;
std::shared_ptr<MetadataCache> MusicController::cache_ = nullptr;
std::shared_ptr<MusicScanner> MusicController::scanner_ = nullptr;

MusicController::MusicController() {
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  fs::create_directories(fs::path(dbPath).parent_path());
  db_ = std::make_shared<MusicDatabase>(dbPath);
  db_->init();
  cache_ = std::make_shared<MetadataCache>();
  scanner_ = std::make_shared<MusicScanner>(*db_, *cache_, "/mnt/media/music");
  auto musicLibraryController = std::make_shared<MusicLibraryController>();
  musicLibraryController->init(db_, cache_);
  auto musicMetadataController = std::make_shared<MusicMetadataController>();
  musicMetadataController->init(db_, cache_);
  auto albumArtController = std::make_shared<AlbumArtController>();
  albumArtController->init(db_);
  auto albumManagementController =
      std::make_shared<AlbumManagementController>();
  albumManagementController->init(db_, cache_);
  auto musicScanController = std::make_shared<MusicScanController>();
  musicScanController->init(db_, cache_, scanner_);
}

void MusicController::init(std::shared_ptr<PlayerController> playerController) {
  MusicPlaybackController::init(db_, playerController);
}
