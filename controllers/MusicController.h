#pragma once

#include "controllers/PlayerController.h"
#include "services/music/MusicScanner.h"
#include <drogon/drogon.h>
#include <memory>

class MusicController : public drogon::HttpController<MusicController> {
public:
  METHOD_LIST_BEGIN
  METHOD_LIST_END

  MusicController();
  static void init(std::shared_ptr<PlayerController> playerController);

private:
  static std::unique_ptr<MusicDatabase> db_;
  static std::unique_ptr<MetadataCache> cache_;
  static std::unique_ptr<MusicScanner> scanner_;
};
