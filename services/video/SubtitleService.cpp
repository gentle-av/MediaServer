#include "SubtitleService.h"
#include "models/SubtitleTrackInfo.h"
#include "services/video/PlaybackService.h"
#include <iostream>

std::expected<std::vector<SubtitleService::SubtitleTrack>, std::string>
SubtitleService::getSubtitleTracks() {
  auto &playbackService = PlaybackService::getInstance();
  std::string activeSocket = playbackService.getActiveSocket();
  if (activeSocket.empty()) {
    return std::unexpected("No active video playing");
  }
  auto tracks = playbackService.getSubtitleTracksFromMpv();
  if (!tracks.has_value()) {
    return std::unexpected(tracks.error());
  }
  std::vector<SubtitleTrack> result;
  result.reserve(tracks->size());
  for (const auto &t : *tracks) {
    SubtitleTrack info;
    info.streamIndex = t.id;
    info.codecName = t.codec;
    info.language = t.lang;
    info.title = t.title;
    info.isDefault = false;
    info.isForced = false;
    result.push_back(std::move(info));
  }
  return result;
}

std::expected<bool, std::string>
SubtitleService::setSubtitleTrack(int streamIndex) {
  auto &playbackService = PlaybackService::getInstance();
  std::string activeSocket = playbackService.getActiveSocket();
  if (activeSocket.empty()) {
    return std::unexpected("No active video playing");
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  if (!playbackService.setSubtitleTrack(streamIndex)) {
    return std::unexpected("Failed to set subtitle track");
  }
  return true;
}

std::expected<std::optional<SubtitleService::SubtitleTrack>, std::string>
SubtitleService::getCurrentSubtitleTrack() {
  auto &playbackService = PlaybackService::getInstance();
  std::string activeSocket = playbackService.getActiveSocket();
  if (activeSocket.empty()) {
    return std::unexpected("No active video playing");
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  std::string value;
  if (!playbackService.getProperty(activeSocket, "sid", value)) {
    return std::optional<SubtitleTrack>{};
  }
  int sid = -1;
  try {
    size_t pos = value.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = value.find(":", pos);
      if (start != std::string::npos) {
        std::string raw = value.substr(start + 1);
        size_t end = raw.find_first_of(",}\n\r\"");
        if (end != std::string::npos) {
          raw = raw.substr(0, end);
        }
        if (raw == "no" || raw == "null" || raw.empty()) {
          sid = -1;
        } else {
          sid = std::stoi(raw);
        }
      }
    }
  } catch (...) {
    sid = -1;
  }
  if (sid <= 0) {
    return std::optional<SubtitleTrack>{};
  }
  auto trackInfo = playbackService.getTrackById(sid, "sub");
  if (!trackInfo.has_value()) {
    return std::optional<SubtitleTrack>{};
  }
  SubtitleTrack info;
  info.streamIndex = trackInfo->id;
  info.codecName = trackInfo->codec;
  info.language = trackInfo->lang;
  info.title = trackInfo->title;
  info.isDefault = false;
  info.isForced = false;
  return std::optional<SubtitleTrack>{info};
}

std::expected<bool, std::string> SubtitleService::disableSubtitles() {
  auto &playbackService = PlaybackService::getInstance();
  std::string activeSocket = playbackService.getActiveSocket();
  if (activeSocket.empty()) {
    return std::unexpected("No active video playing");
  }
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  if (!playbackService.setSubtitleTrack(-1)) {
    return std::unexpected("Failed to disable subtitles");
  }
  return true;
}
