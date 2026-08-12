#include "database/MusicDatabase.h"
#include "database/PlaylistDatabase.h"
#include "profilers/Profiler.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include "services/music/PlaylistManager.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

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
  if (!allTracks || allTracks->empty()) {
    std::cout << "No tracks found in database!" << std::endl;
    return 1;
  }
  std::cout << "Found " << allTracks->size() << " tracks in database\n";
  for (size_t i = 0; i < std::min(size_t(5), allTracks->size()); ++i) {
    const auto &track = (*allTracks)[i];
    std::cout << " - " << track.title << " by " << track.artist << " ("
              << track.album << ")" << std::endl;
  }
  auto playlistDatabase = std::make_unique<PlaylistDatabase>(
      profiler->getDatabasePath() + "/playlists.db");
  playlistDatabase->init();
  auto playlistRepository = std::make_unique<PlaylistRepository>(
      std::move(playlistDatabase), std::move(musicRepository));
  PlaylistManager playlistManager;
  std::string searchQuery = "Metallica";
  std::cout << "\nSearching for: " << searchQuery << std::endl;
  auto playlist = playlistRepository->createFromSearch(searchQuery);
  if (playlist.empty()) {
    std::cout << "No tracks found for: " << searchQuery << std::endl;
    return 1;
  }
  std::cout << "Found " << playlist.size() << " tracks" << std::endl;
  playlistRepository->savePlaylist("New Playlist", playlist);
  std::cout << "\nStarting playback..." << std::endl;
  playlistManager.setLoopMode(true);
  playlistManager.playPlaylist(playlist);
  std::cout << "\n=== Playback Controls ===" << std::endl;
  std::cout << "Playlist: New Playlist (" << playlist.size() << " tracks)"
            << std::endl;
  std::cout << "Loop mode: ON" << std::endl;
  for (int i = 0; i < 30; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto status = playlistManager.getStatus();
    if (status.get("playing", false).asBool()) {
      double current = status.get("currentTime", 0.0).asDouble();
      double duration = status.get("duration", 0.0).asDouble();
      bool paused = status.get("paused", false).asBool();
      std::cout << "\r[" << (paused ? "PAUSED" : "PLAYING") << "] " << current
                << "s / " << duration << "s" << std::flush;
    } else {
      std::cout << "\r[STOPPED] Waiting for track..." << std::flush;
    }
  }
  std::cout << "\n\nStopping playback..." << std::endl;
  playlistManager.stop();
  std::cout << "Done." << std::endl;
  return 0;
}
