#pragma once

#include "database/MusicDatabase.h"
#include <drogon/drogon.h>
#include <string>
#include <vector>

class AlbumArtService {
public:
  AlbumArtService(MusicDatabase &db);
  struct AlbumArt {
    std::vector<char> data;
    std::string mimeType;
  };
  AlbumArt getAlbumArt(const std::string &filePath);
  AlbumArt getAlbumArtByAlbum(const std::string &album,
                              const std::string &artist = "");
  bool saveAlbumArt(const std::string &filePath, const std::vector<char> &data);
  bool removeAlbumArt(const std::string &filePath);
  drogon::HttpResponsePtr createImageResponse(const AlbumArt &albumArt);
  MusicDatabase *getDatabase() { return &db_; }

private:
  std::string detectMimeType(const std::vector<char> &data);
  MusicDatabase &db_;
};
