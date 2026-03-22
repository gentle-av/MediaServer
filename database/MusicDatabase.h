#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct AlbumArtData {
  std::vector<char> data;
  std::string mimeType;
};

struct TrackMetadata {
  std::string path;
  std::string title;
  std::string artist;
  std::string album;
  int trackNumber;
  int duration;
  int bitrate;
  int sampleRate;
  uintmax_t fileSize;
  std::time_t modifiedTime;
  AlbumArtData albumArt;
};

class MusicDatabase {
public:
  MusicDatabase(const std::string &dbPath);
  ~MusicDatabase();
  bool initialize();
  void scanDirectory(
      const std::string &rootPath,
      std::function<void(const std::string &)> progressCallback = nullptr);
  std::vector<std::tuple<std::string, std::string, std::string>>
  getAlbums(const std::string &artistFilter = "");
  std::vector<std::string> getArtists();
  std::vector<TrackMetadata> getTracksByAlbum(const std::string &albumName,
                                              const std::string &artistName);
  AlbumArtData getAlbumArt(const std::string &path);
  bool isTrackCached(const std::string &path, std::time_t modTime);
  void saveTrack(const TrackMetadata &track);
  void forceRescan(const std::string &rootPath);

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
