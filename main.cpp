#include "controllers/MusicController.h"
#include "controllers/PlayerController.h"
#include "controllers/VideoController.h"
#include "player/Player.h"
#include "profilers/Profiler.h"
#include "services/PlayerService.h"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <drogon/drogon.h>
#include <iostream>
#include <memory>
#include <thread>

std::shared_ptr<PlayerService> g_playerService;
std::shared_ptr<Player> g_player;

void signalHandler(int signal) {
  std::cout << "\n[INFO] Received signal " << signal << ", shutting down..."
            << std::endl;
  if (g_player) {
    g_player->forceQuit();
  }
  if (g_playerService) {
    g_playerService->stopAll();
  }
  drogon::app().quit();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "[INFO] Shutdown complete" << std::endl;
  std::exit(0);
}

int main(int argc, char *argv[]) {
  std::cout << "[DEBUG] main started" << std::endl;
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  std::signal(SIGQUIT, signalHandler);
  std::cout << "[DEBUG] Signals registered" << std::endl;
  try {
    Profiler profiler(argc, argv);
    std::cout << "[DEBUG] Profiler created" << std::endl;
    auto config = profiler.getConfig();
    std::cout << "[DEBUG] Config loaded, playerPort: " << config.playerPort
              << std::endl;
    std::cout << "[DEBUG] Creating Player..." << std::endl;
    auto player = std::make_shared<Player>();
    if (!player->isValid()) {
      std::cerr << "[ERROR] Player failed to initialize" << std::endl;
      return 1;
    }
    std::cout << "[DEBUG] Player created and valid" << std::endl;
    g_player = player;
    std::cout << "[DEBUG] Creating PlayerService..." << std::endl;
    auto playerService = std::make_shared<PlayerService>(config.playerPort);
    std::cout << "[DEBUG] PlayerService created" << std::endl;
    playerService->setInternalPlayer(player);
    playerService->setUseInternalPlayer(true);
    std::cout << "[DEBUG] PlayerService configured" << std::endl;
    g_playerService = playerService;
    MusicController::setPlayerService(playerService);
    PlayerController::setPlayerService(playerService);
    VideoController::setPlayerService(playerService);
    std::cout << "[DEBUG] Controllers configured" << std::endl;

    profiler.applyToDrogon(drogon::app());
    std::cout << "[DEBUG] Drogon configured" << std::endl;
    profiler.printStartupInfo();
    std::cout << "[DEBUG] Starting drogon app..." << std::endl;
    drogon::app().run();
    std::cout << "[DEBUG] Drogon stopped" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
