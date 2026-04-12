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
    std::cout << "[DEBUG] Creating audioPlayer..." << std::endl;
    auto audioPlayer = std::make_shared<Player>(false);
    std::cout << "[DEBUG] Audio player created" << std::endl;
    std::cout << "[DEBUG] Creating videoPlayer..." << std::endl;
    auto videoPlayer = std::make_shared<Player>(true);
    std::cout << "[DEBUG] Video player created" << std::endl;
    std::cout << "[DEBUG] Creating PlayerService..." << std::endl;
    auto playerService = std::make_shared<PlayerService>(config.playerPort);
    std::cout << "[DEBUG] PlayerService created" << std::endl;
    playerService->setAudioPlayer(audioPlayer);
    playerService->setVideoPlayer(videoPlayer);
    playerService->setInternalPlayer(audioPlayer);
    playerService->setUseInternalPlayer(true);
    std::cout << "[DEBUG] PlayerService configured" << std::endl;
    g_audioPlayer = audioPlayer;
    g_videoPlayer = videoPlayer;
    g_playerService = playerService;
    MusicController::setPlayerService(playerService);
    PlayerController::setPlayerService(playerService);
    std::cout << "[DEBUG] Controllers configured" << std::endl;
    profiler.applyToDrogon(drogon::app());
    std::cout << "[DEBUG] Drogon configured" << std::endl;
    profiler.printStartupInfo();
    std::cout << "[DEBUG] Starting drogon app..." << std::endl;
    try {
      drogon::app().run();
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] Drogon app exception: " << e.what() << std::endl;
      return 1;
    } catch (...) {
      std::cerr << "[ERROR] Drogon app unknown exception" << std::endl;
      return 1;
    }
    std::cout << "[DEBUG] Drogon stopped" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
