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
  nlohmann::json handleForceRescan(const nlohmann::json &data);
  nlohmann::json handleRemoveMissing(const nlohmann::json &data);
  nlohmann::json handleScanStatus(const nlohmann::json &data);
};
