#pragma once

#include "database/MusicDatabase.h"
#include <string>
#include <vector>

class PlaylistManager {
public:
  PlaylistManager(MusicDatabase &db);

  bool openMusium(const std::vector<std::string> &tracks);
  bool openMusiumByAlbum(const std::string &album,
                         const std::string &artist = "");
  bool openMusiumByArtist(const std::string &artist);

  std::vector<std::string>
  validateTracks(const std::vector<std::string> &tracks);

private:
  MusicDatabase &db_;
};
