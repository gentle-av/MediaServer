#pragma once

#include "controllers/albums/AlbumArtController.h"
#include "controllers/albums/AlbumManagementController.h"
#include "controllers/music/MusicLibraryController.h"
#include "controllers/music/MusicMetadataController.h"
#include "controllers/music/MusicScanController.h"
#include "controllers/player/PlayerController.h"
#include "services/music/MusicScanner.h"
#include <memory>

class MusicController {
public:
  MusicController();
  static void init(std::shared_ptr<PlayerController> playerController);

private:
  static std::shared_ptr<MusicDatabase> db_;
  static std::shared_ptr<MetadataCache> cache_;
  static std::shared_ptr<MusicScanner> scanner_;
  static std::shared_ptr<MusicLibraryController> libraryController_;
  static std::shared_ptr<MusicMetadataController> metadataController_;
  static std::shared_ptr<AlbumArtController> albumArtController_;
  static std::shared_ptr<AlbumManagementController> albumManagementController_;
  static std::shared_ptr<MusicScanController> scanController_;
};
