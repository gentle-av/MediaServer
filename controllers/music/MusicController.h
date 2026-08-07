#pragma once

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
};
