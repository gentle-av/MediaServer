#pragma once

#include "models/MusicMetadata.h"
#include <drogon/drogon.h>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
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
  std::vector<std::string> getAllFiles();
  bool addFile(const std::string &filePath, const MusicMetadata &metadata);
  bool removeFile(const std::string &filePath);
  bool getMetadata(const std::string &filePath, MusicMetadata &metadata);
  bool fileExists(const std::string &filePath);
  bool saveAlbumArt(const std::string &filePath,
                    const std::vector<char> &albumArt);
  AlbumArtData getAlbumArt(const std::string &filePath);
  std::vector<std::string> getArtists();
  std::vector<std::tuple<std::string, std::string, std::string>>
  getAlbums(const std::string &artistFilter = "");
  std::vector<MusicMetadata>
  getTracksByAlbum(const std::string &albumName,
                   const std::string &artistName = "");
  std::vector<MusicMetadata> getTracksByArtist(const std::string &artistName);
  void scanDirectory(const std::string &path,
                     std::function<void(const std::string &)> callback);
  void forceRescan(const std::string &rootPath);
  std::string getFilePathByAlbum(const std::string &albumName,
                                 const std::string &artistName = "");
  bool removeAlbumArt(const std::string &filePath);

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
