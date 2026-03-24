#pragma once
#include <drogon/drogon.h>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

struct MusicMetadata {
  std::string title;
  std::string artist;
  std::string album;
  int duration = 0;
  int track = 0;
  int year = 0;
  std::string genre;
  std::string filePath;
};

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

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
