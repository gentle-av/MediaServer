#include "core/MediaServerCore.h"
#include <csignal>
#include <memory>
#include <thread>

std::unique_ptr<MediaServerCore> server;

void signalHandler(int) {
  if (server) {
    server->shutdown();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  try {
    server = std::make_unique<MediaServerCore>(argc, argv);
    if (!server->run()) {
      return 1;
    }
    while (server->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
