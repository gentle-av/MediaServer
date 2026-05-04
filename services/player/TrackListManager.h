#pragma once

#include <functional>
#include <string>
#include <vector>

class TrackListManager {
public:
  using LoadTrackFunc = std::function<void(int)>;
  explicit TrackListManager(LoadTrackFunc loadTrack);
  void setTrackList(const std::vector<std::string> &tracks);
  void addTrack(const std::string &track);
  void clearTrackList();
  void removeTrack(int index);
  std::vector<std::string> getTrackList() const;
  bool hasTrack(int index) const;
  std::string getTrack(int index) const;
  int size() const;

private:
  std::vector<std::string> tracks_;
  LoadTrackFunc loadTrack_;
};
