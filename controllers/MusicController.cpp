#include "controllers/MusicController.h"
#include "controllers/AlbumArtController.h"
#include "controllers/AlbumManagementController.h"
#include "controllers/MusicLibraryController.h"
#include "controllers/MusicMetadataController.h"
#include "controllers/MusicPlaybackController.h"
#include "controllers/MusicScanController.h"
#include <filesystem>

namespace fs = std::filesystem;

std::unique_ptr<MusicDatabase> MusicController::db_;
std::unique_ptr<MetadataCache> MusicController::cache_;
std::unique_ptr<MusicScanner> MusicController::scanner_;

MusicController::MusicController() {
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  fs::create_directories(fs::path(dbPath).parent_path());
  db_ = std::make_unique<MusicDatabase>(dbPath);
  db_->init();
  cache_ = std::make_unique<MetadataCache>();
  scanner_ = std::make_unique<MusicScanner>(*db_, *cache_, "/mnt/media/music");
  MusicLibraryController::init(db_, cache_);
  MusicMetadataController::init(db_, cache_);
  AlbumArtController::init(db_);
  MusicScanController::init(db_, cache_, scanner_);
  AlbumManagementController::init(db_, cache_);
}

void MusicController::init(std::shared_ptr<PlayerController> playerController) {
  MusicPlaybackController::init(db_, playerController);
}
