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
  bool isLooping() const { return isLooping_; }
  bool isShuffled() const { return isShuffled_; }

private:
  bool loadTrack(int index);
  void onTrackFinished();
  std::vector<std::string> getFilePaths() const;

  std::vector<MusicMetadata> currentPlaylist_;
  int currentIndex_ = -1;
  bool isLooping_ = false;
  bool isShuffled_ = false;

  std::string activeSocket_;

  VideoControlHandler &videoControl_ = VideoControlHandler::getInstance();
  PlaybackStatus &playbackStatus_ = PlaybackStatus::getInstance();
};
