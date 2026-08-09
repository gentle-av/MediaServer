#pragma once

#include "../models/MusicMetadata.h"
#include <memory>
#include <string>
#include <vector>

struct AlbumArtData {
  std::vector<char> data;
  std::string mimeType;
};

class MusicDatabase {
public:
  explicit MusicDatabase(const std::string &dbPath);
  ~MusicDatabase();
  bool init();
  void close();
  bool addFile(const std::string &filePath, const MusicMetadata &metadata);
  bool removeFile(const std::string &filePath);
  bool fileExists(const std::string &filePath);
  bool getMetadata(const std::string &filePath, MusicMetadata &metadata);
  std::vector<std::string> getAllFilePaths();
  bool saveAlbumArt(const std::string &filePath,
                    const std::vector<char> &albumArt);
  AlbumArtData getAlbumArt(const std::string &filePath);
  bool removeAlbumArt(const std::string &filePath);
  std::vector<MusicMetadata>
  getTracksByArtistRaw(const std::string &artistName);
  std::vector<MusicMetadata>
  getTracksByAlbumRaw(const std::string &albumName,
                      const std::string &artistName = "");
  std::vector<std::string> getArtistsRaw();
  std::vector<std::tuple<std::string, std::string, std::string>>
  getAlbumsRaw(const std::string &artistFilter = "");
  std::string getFilePathByAlbumRaw(const std::string &albumName,
                                    const std::string &artistName = "");

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
