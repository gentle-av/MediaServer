#include "services/player/TrackListManager.h"

TrackListManager::TrackListManager(LoadTrackFunc loadTrack)
    : loadTrack_(loadTrack) {}

void TrackListManager::setTrackList(const std::vector<std::string> &tracks) {
  tracks_ = tracks;
  if (!tracks_.empty() && loadTrack_)
    loadTrack_(0);
}

void TrackListManager::addTrack(const std::string &track) {
  tracks_.push_back(track);
}

void TrackListManager::clearTrackList() { tracks_.clear(); }

void TrackListManager::removeTrack(int index) {
  if (index >= 0 && index < (int)tracks_.size())
    tracks_.erase(tracks_.begin() + index);
}

std::vector<std::string> TrackListManager::getTrackList() const {
  return tracks_;
}

bool TrackListManager::hasTrack(int index) const {
  return index >= 0 && index < (int)tracks_.size();
}

std::string TrackListManager::getTrack(int index) const {
  return hasTrack(index) ? tracks_[index] : "";
}

int TrackListManager::size() const { return (int)tracks_.size(); }
