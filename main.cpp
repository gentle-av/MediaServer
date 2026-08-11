#include "database/MusicDatabase.h"
#include "database/PlaylistDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include <memory>

int main(int argc, char *argv[]) {
  auto profiler = std::make_unique<Profiler>(argc, argv);
  auto musicDatabase = std::make_unique<MusicDatabase>(
      profiler->getDatabasePath() + "/music.db");
  auto musicRepository =
      std::make_unique<MusicRepository>(std::move(musicDatabase));
  auto playlistDatabase = std::make_unique<PlaylistDatabase>(
      profiler->getDatabasePath() + "/playlists.db");
  auto playlistRepository = std::make_unique<PlaylistRepository>(
      std::move(playlistDatabase), std::move(musicRepository));
  return 0;
}
