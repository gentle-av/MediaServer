#include "profilers/Profiler.h"
#include <atomic>
#include <csignal>
#include <drogon/drogon.h>
#include <iostream>

std::atomic<bool> running{true};

void signalHandler(int signum) {
  std::cout << "\nInterrupt signal (" << signum << ") received.\n";
  running = false;
  drogon::app().quit();
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  try {
    Profiler profiler(argc, argv);
    auto &app = drogon::app();
    profiler.applyToDrogon(app);
    profiler.printStartupInfo();
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
