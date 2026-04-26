#include "controllers/MusicController.h"
#include "controllers/PlayerController.h"
#include "controllers/VideoController.h"
#include "database/MusicDatabase.h"
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
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  auto musicDb = std::make_shared<MusicDatabase>(dbPath);
  musicDb->init();
  auto player = std::make_shared<Player>();
  auto playerService = std::make_shared<PlayerService>(config.playerPort);
  playerService->setInternalPlayer(player);
  playerService->setMusicDatabase(musicDb);
  g_playerService = playerService;
  MusicController::setPlayerService(playerService);
  PlayerController::setPlayerService(playerService);
  VideoController::setPlayerService(playerService);
  profiler.applyToDrogon(drogon::app());
  profiler.printStartupInfo();
  drogon::app().enableGzip(true).setThreadNum(4).run();
  return 0;
}
