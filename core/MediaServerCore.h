#pragma once
#include "html-server/app/App.h"
#include "profilers/Profiler.h"
#include <atomic>
#include <memory>
#include <thread>

class MusicDatabase;
class PlaylistDatabase;
class MusicRepository;
class PlaylistRepository;
class MetadataCache;
class MusicLibraryController;
class MusicScanController;
class PlaylistController;
class PlayerController;
class VideoController;
class PowerController;
class AlbumArtController;
class AlbumManagementController;

class MediaServerCore {
public:
  MediaServerCore(int argc, char *argv[]);
  ~MediaServerCore();
  bool run();
  void shutdown();
  bool isRunning() const { return running; }

private:
  bool initialize();
  bool initializeDatabases();
  bool initializeRepositories();
  bool initializeServices();
  bool initializeServer();
  bool initializeCors();
  bool initializeControllers();
  bool startServer();
  void runMainLoop();

  std::unique_ptr<Profiler> profiler;
  ProfileConfig config;
  std::unique_ptr<MusicDatabase> musicDb;
  std::unique_ptr<PlaylistDatabase> playlistDb;
  std::shared_ptr<MusicRepository> musicRepo;
  std::shared_ptr<PlaylistRepository> playlistRepo;
  std::shared_ptr<MetadataCache> cache;
  std::unique_ptr<App> app;
  std::unique_ptr<MusicLibraryController> libraryController;
  std::unique_ptr<MusicScanController> scanController;
  std::unique_ptr<PlaylistController> playlistController;
  std::unique_ptr<PlayerController> playerController;
  std::unique_ptr<VideoController> videoController;
  std::unique_ptr<PowerController> powerController;
  std::unique_ptr<AlbumArtController> albumArtController;
  std::unique_ptr<AlbumManagementController> albumManagementController;
  std::atomic<bool> running{true};
  std::jthread mainLoopThread;
};
