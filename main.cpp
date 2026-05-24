#include "controllers/music/MusicController.h"
#include "controllers/player/PlayerController.h"
#include "profilers/Profiler.h"
#include <csignal>
#include <drogon/drogon.h>
#include <memory>

Profiler *g_profiler = nullptr;
std::shared_ptr<PlayerController> g_playerController = nullptr;

void signalHandler(int signal) {
  if (g_playerController) {
    g_playerController->handleForceStop(nullptr,
                                        [](const drogon::HttpResponsePtr &) {});
  }
  drogon::app().quit();
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  Profiler profiler(argc, argv);
  g_profiler = &profiler;
  profiler.applyToDrogon(drogon::app());
  profiler.printStartupInfo();
  g_playerController = std::make_shared<PlayerController>();
  MusicController::init(g_playerController);
  MusicController musicController;
  drogon::app().enableGzip(true).setThreadNum(4).run();
  return 0;
}
