// main.cpp
#include "controllers/music/MusicController.h"
#include "controllers/player/PlayerController.h"
#include "controllers/power/PowerController.h"
#include "controllers/system/MonitorController.h"
#include "controllers/video/VideoController.h"
#include "profilers/Profiler.h"
#include <csignal>
#include <drogon/drogon.h>
#include <memory>

std::shared_ptr<PlayerController> g_playerController = nullptr;
std::shared_ptr<Profiler> g_profiler = nullptr;
std::shared_ptr<MusicController> g_musicController = nullptr;

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
  auto profiler = std::make_shared<Profiler>(argc, argv);
  g_profiler = profiler;
  profiler->applyToDrogon(drogon::app());
  profiler->printStartupInfo();
  auto monitorController = std::make_shared<MonitorController>();
  monitorController->init(profiler);
  auto videoController = std::make_shared<VideoController>();
  videoController->init(profiler);
  auto powerController = std::make_shared<PowerController>();
  g_playerController = std::make_shared<PlayerController>();
  g_musicController = std::make_shared<MusicController>();
  MusicController::init(g_playerController);
  drogon::app().enableGzip(true).setThreadNum(4).run();
  return 0;
}
