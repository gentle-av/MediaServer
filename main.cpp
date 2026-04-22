#include "controllers/MusicController.h"
#include "controllers/PlayerController.h"
#include "controllers/VideoController.h"
#include "player/Player.h"
#include "profilers/Profiler.h"
#include "services/PlayerService.h"
#include <csignal>
#include <drogon/drogon.h>
#include <iostream>
#include <memory>

std::shared_ptr<PlayerService> g_playerService;
Profiler *g_profiler = nullptr;

void signalHandler(int signal) {
  std::cout << "\n[INFO] Shutting down..." << std::endl;
  if (g_playerService)
    g_playerService->stop();
  drogon::app().quit();
  std::exit(0);
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  Profiler profiler(argc, argv);
  g_profiler = &profiler;
  auto config = profiler.getConfig();
  auto player = std::make_shared<Player>();
  auto playerService = std::make_shared<PlayerService>(config.playerPort);
  playerService->setInternalPlayer(player);
  g_playerService = playerService;
  MusicController::setPlayerService(playerService);
  PlayerController::setPlayerService(playerService);
  profiler.applyToDrogon(drogon::app());
  profiler.printStartupInfo();
  drogon::app().run();
  return 0;
}
