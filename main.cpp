#include "controllers/MusicController.h"
#include "controllers/PlayerController.h"
#include "database/MusicDatabase.h"
#include "profilers/Profiler.h"
#include <csignal>
#include <drogon/drogon.h>
#include <iostream>
#include <memory>

Profiler *g_profiler = nullptr;
std::shared_ptr<PlayerController> g_playerController = nullptr;

void signalHandler(int signal) {
  std::cout << "\n[INFO] Shutting down..." << std::endl;
  if (g_playerController) {
    g_playerController->handleForceStop(nullptr,
                                        [](const drogon::HttpResponsePtr &) {});
  }
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
  profiler.applyToDrogon(drogon::app());
  profiler.printStartupInfo();
  g_playerController = std::make_shared<PlayerController>();
  MusicController::setPlayerController(g_playerController);
  drogon::app().enableGzip(true).setThreadNum(4).run();
  return 0;
}
