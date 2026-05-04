#include "services/music/PlaylistManager.h"
#include "services/music/MetadataExtractor.h"
#include <drogon/drogon.h>

PlaylistManager::PlaylistManager(
    std::shared_ptr<PlayerController> playerController, MusicDatabase &db)
    : playerController_(playerController), db_(db) {}

std::vector<std::string>
PlaylistManager::validateTracks(const std::vector<std::string> &tracks) {
  std::vector<std::string> validTracks;
  for (const auto &track : tracks) {
    MusicMetadata meta;
    if (MetadataExtractor::extractMetadata(track, meta) && meta.duration > 0) {
      validTracks.push_back(track);
    }
  }
  return validTracks;
}

bool PlaylistManager::openMusium(const std::vector<std::string> &tracks) {
  if (!playerController_)
    return false;

  auto validTracks = validateTracks(tracks);
  if (validTracks.empty())
    return false;

  Json::Value playlistJson;
  for (const auto &track : validTracks) {
    playlistJson.append(track);
  }

  Json::Value setPlaylistReq;
  setPlaylistReq["tracks"] = playlistJson;
  auto mockReq = drogon::HttpRequest::newHttpJsonRequest(setPlaylistReq);

  playerController_->handleSetPlaylist(mockReq,
                                       [](const drogon::HttpResponsePtr &) {});
  playerController_->handlePlay(mockReq,
                                [](const drogon::HttpResponsePtr &) {});

  return true;
}

bool PlaylistManager::openMusiumByAlbum(const std::string &album,
                                        const std::string &artist) {
  auto tracks = db_.getTracksByAlbum(album, artist);
  std::vector<std::string> trackPaths;
  for (const auto &track : tracks) {
    trackPaths.push_back(track.filePath);
  }
  return openMusium(trackPaths);
}

bool PlaylistManager::openMusiumByArtist(const std::string &artist) {
  auto tracks = db_.getTracksByArtist(artist);
  std::vector<std::string> trackPaths;
  for (const auto &track : tracks) {
    trackPaths.push_back(track.filePath);
  }
  return openMusium(trackPaths);
}
