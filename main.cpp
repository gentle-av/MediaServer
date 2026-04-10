#include "controllers/MusicController.h"
#include "controllers/PlayerController.h"
#include "controllers/VideoController.h"
#include "player/Player.h"
#include "profilers/Profiler.h"
#include "services/PlayerService.h"
#include <drogon/drogon.h>
#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
  try {
    Profiler profiler(argc, argv);
    auto config = profiler.getConfig();
    auto player = std::make_shared<Player>();
    if (!player->start()) {
      std::cerr << "Failed to start local player" << std::endl;
      return 1;
    }
    auto playerService = std::make_shared<PlayerService>(config.playerPort);
    playerService->setInternalPlayer(player);
    playerService->setUseInternalPlayer(true);
    playerService->ensureConnection();
    MusicController::setPlayerService(playerService);
    PlayerController::setPlayerService(playerService);
    profiler.applyToDrogon(drogon::app());
    profiler.printStartupInfo();
    drogon::app().run();
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
