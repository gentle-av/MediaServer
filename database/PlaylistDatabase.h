#pragma once

#include "../models/Playlist.h"
#include <memory>
#include <string>
#include <vector>

class PlaylistDatabase {
public:
  explicit PlaylistDatabase(const std::string &dbPath);
  ~PlaylistDatabase();

  bool init();
  void close();

  bool savePlaylist(const std::string &playlistName, const Playlist &playlist);
  bool savePlaylist(const std::string &playlistName,
                    const std::vector<MusicMetadata> &tracks);

  std::optional<Playlist> loadPlaylist(const std::string &playlistName);
  bool loadPlaylist(const std::string &playlistName, Playlist &playlist);
  bool deletePlaylist(const std::string &playlistName);
  bool playlistExists(const std::string &playlistName) const;

  std::vector<std::string> getAllPlaylistNames() const;
  int getTrackCount(const std::string &playlistName) const;

  bool addTrackToPlaylist(const std::string &playlistName,
                          const MusicMetadata &track);
  bool addTrackToPlaylist(const std::string &playlistName,
                          const std::string &filePath);
  bool removeTrackFromPlaylist(const std::string &playlistName, int index);
  bool clearPlaylist(const std::string &playlistName);
  bool shufflePlaylist(const std::string &playlistName);

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
