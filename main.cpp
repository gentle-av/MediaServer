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
std::shared_ptr<Player> g_audioPlayer;
std::shared_ptr<Player> g_videoPlayer;

void signalHandler(int signal) {
  std::cout << "\n[INFO] Received signal " << signal << ", shutting down..."
            << std::endl;
  if (g_audioPlayer) {
    g_audioPlayer->forceQuit();
  }
  if (g_videoPlayer) {
    g_videoPlayer->forceQuit();
  }
  if (g_playerService) {
    g_playerService->stopAll();
  }
  drogon::app().quit();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "[INFO] Shutdown complete" << std::endl;
  exit(0);
}

int main(int argc, char *argv[]) {
  try {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGQUIT, signalHandler);
    Profiler profiler(argc, argv);
    auto config = profiler.getConfig();
    auto audioPlayer = std::make_shared<Player>(false);
    auto videoPlayer = std::make_shared<Player>(true);
    g_audioPlayer = audioPlayer;
    g_videoPlayer = videoPlayer;
    if (!audioPlayer->start()) {
      std::cerr << "Failed to start audio player" << std::endl;
      return 1;
    }
    if (!videoPlayer->start()) {
      std::cerr << "Failed to start video player" << std::endl;
      return 1;
    }
    auto playerService = std::make_shared<PlayerService>(config.playerPort);
    playerService->setInternalPlayer(audioPlayer);
    playerService->setUseInternalPlayer(true);
    playerService->ensureConnection();
    g_playerService = playerService;
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
