#include "database/MusicDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    Profiler profiler(argc, argv);
    profiler.printStartupInfo();
    auto config = profiler.getConfig();
    MusicDatabase db(config.databasePath + "/music.db");
    if (!db.init()) {
      std::cerr << "Failed to initialize database" << std::endl;
      return 1;
    }
    std::cout << "Database initialized successfully" << std::endl;
    MusicRepository repository(db);
    repository.setTTL(std::chrono::seconds(60));
    std::cout << "\nMusic directory: " << config.musicDirectory << std::endl;
    std::cout << "Repository cache size: " << repository.getCacheSize()
              << std::endl;
    std::cout << "\nStarting scan..." << std::endl;
    auto future = repository.scanMusicDirectoryAsync(
        config.musicDirectory, [](int total, int processed) {
          if (processed % 100 == 0 || processed == total) {
            std::cout << "\rProgress: " << processed << "/" << total << " files"
                      << std::flush;
          }
        });
    while (repository.isScanning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    bool success = future.get();
    std::cout << "\nScan " << (success ? "completed" : "failed") << std::endl;
    if (success) {
      auto artists = repository.getArtists();
      std::cout << "Artists in library: " << artists.size() << std::endl;
      for (const auto &artist : artists) {
        std::cout << "  - " << artist << std::endl;
      }
    }
    std::cout << "\nRepository ready" << std::endl;
    std::cout << "Cache size: " << repository.getCacheSize() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
