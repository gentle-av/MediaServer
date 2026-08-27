#pragma once

#include "../../models/Playlist.h"
#include "../video/PlaybackStatus.h"
#include "../video/VideoControlHandler.h"
#include <json/value.h>
#include <string>
#include <vector>

class PlaylistManager {
public:
  PlaylistManager() = default;
  ~PlaylistManager() = default;

  bool playPlaylist(const Playlist &playlist);
  bool playPlaylist(Playlist &&playlist);

  bool playTracks(const std::vector<MusicMetadata> &tracks);
  bool playTracks(std::vector<MusicMetadata> &&tracks);

  bool playFilePaths(const std::vector<std::string> &paths);

  void pause();
  void resume();
  void togglePause();
  void stop();
  bool seek(double seconds);
  bool next();
  bool previous();

  bool setVolume(int percent);
  int getVolume() const;
  bool toggleMute();
  bool isMuted() const;

  bool isPlaying() const;
  Json::Value getStatus() const;
  double getCurrentTime() const;
  double getDuration() const;

  void setLoopMode(bool enabled);
  void setShuffleMode(bool enabled);
  bool isLooping() const { return looping; }
  bool isShuffled() const { return shuffled; }

private:
  bool loadTrack(int index);
  void onTrackFinished();
  std::vector<std::string> getFilePaths() const;

  std::vector<MusicMetadata> currentPlaylist;
  int currentIndex = -1;
  bool looping = false;
  bool shuffled = false;

  std::string activeSocket;

  VideoControlHandler &videoControl = VideoControlHandler::getInstance();
  PlaybackStatus &playbackStatus = PlaybackStatus::getInstance();
};
