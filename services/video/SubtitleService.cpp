#include "SubtitleService.h"
#include "models/SubtitleTrackInfo.h"
#include "services/video/PlaybackService.h"

std::expected<std::vector<SubtitleService::SubtitleTrack>, std::string>
SubtitleService::getSubtitleTracks(const std::string &filePath) {
  SubtitleTrackInfo trackInfo;
  auto result = trackInfo.openFile(filePath);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  std::vector<SubtitleTrack> tracks;
  tracks.reserve(trackInfo.getTrackCount());
  for (const auto &track : trackInfo.getSubtitleTracks()) {
    SubtitleTrack info;
    info.streamIndex = track.streamIndex;
    info.codecName = track.codecName;
    info.language = track.language;
    info.title = track.title;
    info.isDefault = track.isDefault;
    info.isForced = track.isForced;
    tracks.push_back(std::move(info));
  }
  return tracks;
}

std::expected<bool, std::string>
SubtitleService::setSubtitleTrack(const std::string &activeSocket,
                                  int streamIndex) {
  auto &playbackService = PlaybackService::getInstance();
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  if (!playbackService.setSubtitleTrack(streamIndex)) {
    return std::unexpected("Failed to set subtitle track");
  }
  return true;
}

std::expected<std::optional<SubtitleService::SubtitleTrack>, std::string>
SubtitleService::getCurrentSubtitleTrack(const std::string &activeSocket,
                                         const std::string &filePath) {
  auto &playbackService = PlaybackService::getInstance();
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  std::string value;
  if (!playbackService.getProperty(activeSocket, "sid", value)) {
    return std::unexpected("Failed to get subtitle track");
  }
  int sid = -1;
  try {
    size_t pos = value.find("\"data\"");
    if (pos != std::string::npos) {
      size_t start = value.find(":", pos);
      if (start != std::string::npos) {
        sid = std::stoi(value.substr(start + 1));
      }
    }
  } catch (...) {
    sid = -1;
  }
  if (sid <= 0) {
    return std::optional<SubtitleTrack>{};
  }
  SubtitleTrackInfo trackInfo;
  auto result = trackInfo.openFile(filePath);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  auto track = trackInfo.getTrackByStreamIndex(sid);
  if (!track.has_value()) {
    return std::optional<SubtitleTrack>{};
  }
  SubtitleTrack info;
  info.streamIndex = track->streamIndex;
  info.codecName = track->codecName;
  info.language = track->language;
  info.title = track->title;
  info.isDefault = track->isDefault;
  info.isForced = track->isForced;
  return std::optional<SubtitleTrack>{info};
}

std::expected<bool, std::string>
SubtitleService::disableSubtitles(const std::string &activeSocket) {
  auto &playbackService = PlaybackService::getInstance();
  if (!playbackService.checkProcessAlive(activeSocket)) {
    return std::unexpected("MPV process is dead");
  }
  if (!playbackService.setSubtitleTrack(-1)) {
    return std::unexpected("Failed to disable subtitles");
  }
  return true;
}
