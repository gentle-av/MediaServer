#pragma once

#include "../../repositories/MusicRepository.h"
#include "../../repositories/PlaylistRepository.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>

class MusicScanController : public RestController<App> {
private:
  MusicRepository &musicRepository;
  PlaylistRepository &playlistRepository;
  std::string musicDirectory;

public:
  explicit MusicScanController(App &app, MusicRepository &repo,
                               PlaylistRepository &playlistRepo,
                               const std::string &musicDir);
  ~MusicScanController() = default;

protected:
  void register_all_routes() override;

private:
  nlohmann::json handleForceRescan(const StringHttpRequest &req,
                                   const std::string &targetDir);
  nlohmann::json handleRemoveMissing(const StringHttpRequest &req);
  nlohmann::json handleStartScan(const StringHttpRequest &req);
  nlohmann::json handleScanStatus(const StringHttpRequest &req);

  std::atomic<int> scanTotal{0};
  std::atomic<int> scanProcessed{0};
  std::atomic<int> scanAdded{0};
  std::atomic<int> scanErrors{0};
  std::atomic<bool> scanFinished{false};
  std::atomic<bool> scanSuccess{true};
  std::atomic<bool> scanRunning{false};
};
