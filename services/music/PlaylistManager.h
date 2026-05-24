#pragma once

#include "controllers/player/PlayerController.h"
#include "database/MusicDatabase.h"
#include <memory>
#include <string>
#include <vector>

class PlaylistManager {
public:
  PlaylistManager(std::shared_ptr<PlayerController> playerController,
                  MusicDatabase &db);

  bool openMusium(const std::vector<std::string> &tracks);
  bool openMusiumByAlbum(const std::string &album,
                         const std::string &artist = "");
  bool openMusiumByArtist(const std::string &artist);

  std::vector<std::string>
  validateTracks(const std::vector<std::string> &tracks);

private:
  std::shared_ptr<PlayerController> playerController_;
  MusicDatabase &db_;
};
