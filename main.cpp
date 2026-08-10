#include "database/MusicDatabase.h"
#include "models/Playlist.h"
#include "repositories/MusicRepository.h"
#include "repositories/PlaylistRepository.h"
#include "services/music/MetadataExtractor.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

void printSeparator() {
  std::cout << "\n========================================\n" << std::endl;
}

void printPlaylist(const Playlist &playlist, const std::string &name) {
  std::cout << "Playlist: " << name << std::endl;
  std::cout << "  Tracks: " << playlist.size() << std::endl;
  std::cout << "  Current index: " << playlist.getCurrentIndex() << std::endl;

  auto tracks = playlist.getAllTracks();
  for (size_t i = 0; i < std::min(tracks.size(), size_t(5)); ++i) {
    const auto &track = tracks[i];
    std::cout << "    " << i + 1 << ". " << track.artist << " - " << track.title
              << " (" << track.album << ")" << std::endl;
  }
  if (tracks.size() > 5) {
    std::cout << "    ... and " << (tracks.size() - 5) << " more tracks"
              << std::endl;
  }
  std::cout << std::endl;
}

// Функция для заполнения базы данных тестовыми треками
void populateDatabase(MusicDatabase &db) {
  std::cout << "Populating database with test tracks..." << std::endl;

  // Создаем тестовые треки
  std::vector<MusicMetadata> testTracks;

  // Queen - Bohemian Rhapsody
  MusicMetadata track1;
  track1.filePath = "/music/queen/bohemian_rhapsody.mp3";
  track1.title = "Bohemian Rhapsody";
  track1.artist = "Queen";
  track1.album = "A Night at the Opera";
  track1.duration = 354;
  track1.track = 1;
  track1.year = 1975;
  track1.genre = "Rock";
  testTracks.push_back(track1);

  // Queen - Another One Bites the Dust
  MusicMetadata track2;
  track2.filePath = "/music/queen/another_one_bites_the_dust.mp3";
  track2.title = "Another One Bites the Dust";
  track2.artist = "Queen";
  track2.album = "The Game";
  track2.duration = 215;
  track2.track = 2;
  track2.year = 1980;
  track2.genre = "Rock";
  testTracks.push_back(track2);

  // Queen - We Will Rock You
  MusicMetadata track3;
  track3.filePath = "/music/queen/we_will_rock_you.mp3";
  track3.title = "We Will Rock You";
  track3.artist = "Queen";
  track3.album = "News of the World";
  track3.duration = 122;
  track3.track = 3;
  track3.year = 1977;
  track3.genre = "Rock";
  testTracks.push_back(track3);

  // Queen - We Are the Champions
  MusicMetadata track4;
  track4.filePath = "/music/queen/we_are_the_champions.mp3";
  track4.title = "We Are the Champions";
  track4.artist = "Queen";
  track4.album = "News of the World";
  track4.duration = 179;
  track4.track = 4;
  track4.year = 1977;
  track4.genre = "Rock";
  testTracks.push_back(track4);

  // The Beatles - Hey Jude
  MusicMetadata track5;
  track5.filePath = "/music/beatles/hey_jude.mp3";
  track5.title = "Hey Jude";
  track5.artist = "The Beatles";
  track5.album = "Hey Jude";
  track5.duration = 431;
  track5.track = 1;
  track5.year = 1968;
  track5.genre = "Rock";
  testTracks.push_back(track5);

  // The Beatles - Let It Be
  MusicMetadata track6;
  track6.filePath = "/music/beatles/let_it_be.mp3";
  track6.title = "Let It Be";
  track6.artist = "The Beatles";
  track6.album = "Let It Be";
  track6.duration = 243;
  track6.track = 2;
  track6.year = 1970;
  track6.genre = "Rock";
  testTracks.push_back(track6);

  // Pink Floyd - Another Brick in the Wall
  MusicMetadata track7;
  track7.filePath = "/music/pink_floyd/another_brick_in_the_wall.mp3";
  track7.title = "Another Brick in the Wall";
  track7.artist = "Pink Floyd";
  track7.album = "The Wall";
  track7.duration = 383;
  track7.track = 5;
  track7.year = 1979;
  track7.genre = "Progressive Rock";
  testTracks.push_back(track7);

  // Pink Floyd - Comfortably Numb
  MusicMetadata track8;
  track8.filePath = "/music/pink_floyd/comfortably_numb.mp3";
  track8.title = "Comfortably Numb";
  track8.artist = "Pink Floyd";
  track8.album = "The Wall";
  track8.duration = 383;
  track8.track = 19;
  track8.year = 1979;
  track8.genre = "Progressive Rock";
  testTracks.push_back(track8);

  // Adding tracks to database
  int added = 0;
  for (const auto &track : testTracks) {
    if (db.addFile(track.filePath, track)) {
      added++;
      std::cout << "  Added: " << track.artist << " - " << track.title
                << std::endl;
    }
  }

  std::cout << "Added " << added << " test tracks to database" << std::endl;
  std::cout << std::endl;
}

void testPlaylistOperations() {
  printSeparator();
  std::cout << "TEST: Playlist Operations" << std::endl;

  // Create a playlist with some tracks
  std::vector<MusicMetadata> tracks;

  MusicMetadata track1;
  track1.filePath = "/music/track1.mp3";
  track1.title = "Bohemian Rhapsody";
  track1.artist = "Queen";
  track1.album = "A Night at the Opera";
  track1.duration = 354;
  track1.track = 1;
  track1.year = 1975;
  track1.genre = "Rock";
  tracks.push_back(track1);

  MusicMetadata track2;
  track2.filePath = "/music/track2.mp3";
  track2.title = "Another One Bites the Dust";
  track2.artist = "Queen";
  track2.album = "The Game";
  track2.duration = 215;
  track2.track = 2;
  track2.year = 1980;
  track2.genre = "Rock";
  tracks.push_back(track2);

  MusicMetadata track3;
  track3.filePath = "/music/track3.mp3";
  track3.title = "We Will Rock You";
  track3.artist = "Queen";
  track3.album = "News of the World";
  track3.duration = 122;
  track3.track = 3;
  track3.year = 1977;
  track3.genre = "Rock";
  tracks.push_back(track3);

  Playlist playlist(tracks);
  std::cout << "Created playlist with " << playlist.size() << " tracks"
            << std::endl;

  // Test adding track
  MusicMetadata track4;
  track4.filePath = "/music/track4.mp3";
  track4.title = "We Are the Champions";
  track4.artist = "Queen";
  track4.album = "News of the World";
  track4.duration = 179;
  track4.track = 4;
  track4.year = 1977;
  track4.genre = "Rock";
  playlist.addTrack(track4);
  std::cout << "Added track, now " << playlist.size() << " tracks" << std::endl;

  // Test shuffle
  playlist.shuffle();
  std::cout << "Shuffled playlist" << std::endl;

  // Test getting track
  auto track = playlist.getTrack(0);
  if (track) {
    std::cout << "First track: " << track->artist << " - " << track->title
              << std::endl;
  }

  // Test removing track
  playlist.removeTrack(0);
  std::cout << "Removed first track, now " << playlist.size() << " tracks"
            << std::endl;

  // Test clearing
  playlist.clear();
  std::cout << "Cleared playlist, size: " << playlist.size() << std::endl;
}

void testPlaylistRepository(MusicDatabase &db) {
  printSeparator();
  std::cout << "TEST: Playlist Repository (using real database data)"
            << std::endl;

  PlaylistRepository repo(db);
  repo.setTTL(std::chrono::seconds(30));

  // First, let's check what's in the database
  auto allTracks = db.getAllFilePaths();
  std::cout << "Database contains " << allTracks.size() << " tracks"
            << std::endl;

  // Get artists from database
  auto artists = db.getArtistsRaw();
  std::cout << "Artists in database: ";
  for (const auto &artist : artists) {
    std::cout << artist << " ";
  }
  std::cout << std::endl << std::endl;

  // 1. Create playlist from artist (using real database data)
  if (!artists.empty()) {
    std::string artistName = artists[0];
    std::cout << "1. Creating playlist from artist: " << artistName
              << std::endl;
    if (repo.createPlaylistFromArtist("artist_playlist", artistName)) {
      std::cout << "  ✓ Created playlist: artist_playlist" << std::endl;
      auto playlist = repo.getPlaylist("artist_playlist");
      if (playlist) {
        std::cout << "    Tracks: " << playlist->size() << std::endl;
        auto tracks = playlist->getAllTracks();
        for (const auto &track : tracks) {
          std::cout << "    - " << track.artist << " - " << track.title << " ("
                    << track.album << ")" << std::endl;
        }
      }
    } else {
      std::cout << "  ✗ Failed to create playlist from artist" << std::endl;
    }
  }

  // 2. Create playlist from album
  std::cout << "\n2. Creating playlist from album..." << std::endl;
  auto albums = db.getAlbumsRaw();
  if (!albums.empty()) {
    auto [albumName, artistName, year] = albums[0];
    std::cout << "  Using album: " << albumName << " by " << artistName
              << std::endl;
    if (repo.createPlaylistFromAlbum("album_playlist", albumName, artistName)) {
      std::cout << "  ✓ Created playlist: album_playlist" << std::endl;
      auto playlist = repo.getPlaylist("album_playlist");
      if (playlist) {
        std::cout << "    Tracks: " << playlist->size() << std::endl;
        auto tracks = playlist->getAllTracks();
        for (const auto &track : tracks) {
          std::cout << "    - " << track.artist << " - " << track.title
                    << std::endl;
        }
      }
    } else {
      std::cout << "  ✗ Failed to create playlist from album" << std::endl;
    }
  }

  // 3. Create playlist from search
  std::cout << "\n3. Creating playlist from search..." << std::endl;
  std::string searchQuery = "Rock";
  std::cout << "  Searching for: " << searchQuery << std::endl;
  if (repo.createPlaylistFromSearch("search_playlist", searchQuery)) {
    std::cout << "  ✓ Created playlist: search_playlist" << std::endl;
    auto playlist = repo.getPlaylist("search_playlist");
    if (playlist) {
      std::cout << "    Tracks: " << playlist->size() << std::endl;
      auto tracks = playlist->getAllTracks();
      for (const auto &track : tracks) {
        std::cout << "    - " << track.artist << " - " << track.title
                  << std::endl;
      }
    }
  } else {
    std::cout << "  ✗ Failed to create playlist from search" << std::endl;
  }

  // 4. Get all playlist names
  std::cout << "\n4. Getting playlist names..." << std::endl;
  auto names = repo.getPlaylistNames();
  std::cout << "  Found " << names.size() << " playlists:" << std::endl;
  for (const auto &name : names) {
    std::cout << "    - " << name << std::endl;
  }

  // 5. Export playlist
  if (!names.empty()) {
    std::cout << "\n5. Exporting playlist..." << std::endl;
    if (repo.exportPlaylist(names[0], "exported_playlist.json")) {
      std::cout << "  ✓ Exported playlist to exported_playlist.json"
                << std::endl;
    } else {
      std::cout << "  ✗ Failed to export playlist" << std::endl;
    }
  }

  // 6. Test cache
  std::cout << "\n6. Cache size: " << repo.getCacheSize() << std::endl;
}

void testPlaylistScanning(MusicDatabase &db) {
  printSeparator();
  std::cout << "TEST: Playlist Scanning" << std::endl;

  PlaylistRepository repo(db);

  // Create a test playlists directory with some playlist files
  std::cout << "\nCreating test playlists directory..." << std::endl;
  std::string testDir = "test_playlists";
  std::filesystem::create_directories(testDir);

  // Create some test playlist files using tracks from database
  auto allTracks = db.getAllFilePaths();
  std::vector<MusicMetadata> tracks;

  // Get metadata for tracks from database
  for (const auto &path : allTracks) {
    MusicMetadata meta;
    if (db.getMetadata(path, meta)) {
      tracks.push_back(meta);
    }
  }

  if (!tracks.empty()) {
    // Create a playlist with first 3 tracks
    std::vector<MusicMetadata> playlistTracks;
    for (size_t i = 0; i < std::min(tracks.size(), size_t(3)); ++i) {
      playlistTracks.push_back(tracks[i]);
    }

    Playlist scanPlaylist(playlistTracks);
    scanPlaylist.save(testDir + "/scan_playlist.json");
    std::cout << "  ✓ Created scan_playlist.json with " << playlistTracks.size()
              << " tracks" << std::endl;
  }

  // Start scanning
  std::cout << "\nStarting scan of playlist directory..." << std::endl;
  auto future =
      repo.scanPlaylistDirectoryAsync(testDir, [](int total, int processed) {
        std::cout << "\r  Progress: " << processed << "/" << total << " files"
                  << std::flush;
      });

  // Wait for scan to complete
  while (repo.isScanning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  bool success = future.get();
  std::cout << "\n  Scan " << (success ? "completed" : "failed") << std::endl;

  if (success) {
    // Check imported playlists
    auto names = repo.getPlaylistNames();
    std::cout << "  Imported playlists:" << std::endl;
    for (const auto &name : names) {
      std::cout << "    - " << name << std::endl;
      auto playlist = repo.getPlaylist(name);
      if (playlist) {
        std::cout << "      Tracks: " << playlist->size() << std::endl;
      }
    }
  }

  // Clean up
  std::cout << "\nCleaning up test directory..." << std::endl;
  std::filesystem::remove_all(testDir);
}

void testPlaylistPerformance(MusicDatabase &db) {
  printSeparator();
  std::cout << "TEST: Playlist Performance" << std::endl;

  PlaylistRepository repo(db);

  // Get tracks from database
  auto allTracks = db.getAllFilePaths();
  std::vector<MusicMetadata> tracks;
  for (const auto &path : allTracks) {
    MusicMetadata meta;
    if (db.getMetadata(path, meta)) {
      tracks.push_back(meta);
    }
  }

  if (tracks.empty()) {
    std::cout << "  No tracks in database, skipping performance test"
              << std::endl;
    return;
  }

  // Create many playlists from database tracks
  const int NUM_PLAYLISTS = 10;
  std::cout << "\nCreating " << NUM_PLAYLISTS
            << " playlists from database tracks..." << std::endl;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 1; i <= NUM_PLAYLISTS; ++i) {
    std::vector<MusicMetadata> playlistTracks;
    // Take different tracks for each playlist
    int startIdx = (i - 1) * 2 % tracks.size();
    for (int j = 0; j < 3; ++j) {
      int idx = (startIdx + j) % tracks.size();
      playlistTracks.push_back(tracks[idx]);
    }
    Playlist playlist(playlistTracks);
    repo.savePlaylist("perf_playlist_" + std::to_string(i), playlist);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto createDuration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "  Created " << NUM_PLAYLISTS << " playlists in "
            << createDuration.count() << "ms" << std::endl;

  // Test cache performance
  std::cout << "\nTesting cache performance..." << std::endl;
  start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    auto names = repo.getPlaylistNames();
    if (i == 0) {
      std::cout << "  First call (cache miss): " << names.size() << " playlists"
                << std::endl;
    }
  }

  end = std::chrono::high_resolution_clock::now();
  auto cacheDuration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "  100 cache reads in " << cacheDuration.count() << " μs"
            << std::endl;

  // Clean up
  std::cout << "\nCleaning up performance test playlists..." << std::endl;
  auto names = repo.getPlaylistNames();
  for (const auto &name : names) {
    if (name.find("perf_playlist_") != std::string::npos) {
      repo.deletePlaylist(name);
    }
  }
  std::cout << "  Cleanup complete" << std::endl;
}

int main(int argc, char *argv[]) {
  try {
    std::cout << "========================================" << std::endl;
    std::cout << "  PLAYLIST REPOSITORY TEST SUITE" << std::endl;
    std::cout << "  (Using Real Database Data)" << std::endl;
    std::cout << "========================================" << std::endl;

    // Initialize database
    MusicDatabase db("test_music.db");
    if (!db.init()) {
      std::cerr << "Failed to initialize database" << std::endl;
      return 1;
    }
    std::cout << "Database initialized successfully" << std::endl;

    // Populate database with test tracks
    populateDatabase(db);

    // Run tests
    testPlaylistOperations();
    testPlaylistRepository(db);
    testPlaylistScanning(db);
    testPlaylistPerformance(db);

    // Summary
    printSeparator();
    std::cout << "ALL TESTS COMPLETED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;

    // Clean up
    std::cout << "\nCleaning up..." << std::endl;
    std::filesystem::remove("test_music.db");
    std::filesystem::remove_all("playlists");
    std::filesystem::remove("exported_playlist.json");
    std::cout << "Cleanup complete" << std::endl;

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
