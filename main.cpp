#include "database/MusicDatabase.h"
#include "database/PlaylistDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
  auto profiler = std::make_unique<Profiler>(argc, argv);
  auto musicDatabase = std::make_unique<MusicDatabase>(
      profiler->getDatabasePath() + "/music.db");
  musicDatabase->init();
  auto musicRepository =
      std::make_unique<MusicRepository>(std::move(musicDatabase));
  std::string musicDir = profiler->getMusicDirectory();
  std::cout << "Scanning music directory: " << musicDir << std::endl;
  auto scanFuture = musicRepository->scanMusicDirectoryAsync(
      musicDir, [](int total, int processed) {
        std::cout << "\rProgress: " << processed << "/" << total
                  << " files scanned" << std::flush;
      });
  bool scanSuccess = scanFuture.get();
  std::cout << "\nScan completed: " << (scanSuccess ? "SUCCESS" : "FAILED")
            << std::endl;
  auto allTracks = musicRepository->getAllTracks();
  if (allTracks && !allTracks->empty()) {
    std::cout << "Found " << allTracks->size() << " tracks in database\n";
    for (size_t i = 0; i < std::min(size_t(5), allTracks->size()); ++i) {
      const auto &track = (*allTracks)[i];
      std::cout << " - " << track.title << " by " << track.artist << " ("
                << track.album << ")" << std::endl;
    }
  } else {
    std::cout << "No tracks found in database!" << std::endl;
    std::cout << "Check that music directory contains audio files."
              << std::endl;
    return 1;
  }
  auto playlistDatabase = std::make_unique<PlaylistDatabase>(
      profiler->getDatabasePath() + "/playlists.db");
  playlistDatabase->init();
  auto playlistRepository = std::make_unique<PlaylistRepository>(
      std::move(playlistDatabase), std::move(musicRepository));
  std::string searchQuery = "Metallica";
  std::cout << "\nSearching for: " << searchQuery << std::endl;
  bool added =
      playlistRepository->createPlaylistFromSearch("New Playlist", searchQuery);
  std::cout << "Playlist created: " << std::boolalpha << added << std::endl;
  if (added) {
    auto playlist = playlistRepository->getPlaylist("New Playlist");
    if (playlist) {
      std::cout << "Playlist has " << playlist->size() << " tracks"
                << std::endl;
    }
  }
  return 0;
}
