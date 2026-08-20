#pragma once

#include "../../repositories/MusicRepository.h"
#include "../../repositories/PlaylistRepository.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <nlohmann/json.hpp>

class PlaylistController : public RestController<App> {
public:
  explicit PlaylistController(App &app, PlaylistRepository &playlistRepo,
                              MusicRepository &musicRepo);
  ~PlaylistController() = default;

protected:
  void register_all_routes() override;

private:
  PlaylistRepository &playlistRepository;
  MusicRepository &musicRepository;

  typename App::ResponseType handleGetPlaylists(const App::RequestType &req);
  typename App::ResponseType handleGetPlaylist(const App::RequestType &req);
  typename App::ResponseType handleCreatePlaylist(const App::RequestType &req);
  typename App::ResponseType handleUpdatePlaylist(const App::RequestType &req);
  typename App::ResponseType handleDeletePlaylist(const App::RequestType &req);
  typename App::ResponseType handleRenamePlaylist(const App::RequestType &req);
  typename App::ResponseType handleAddTracks(const App::RequestType &req);
  typename App::ResponseType handleRemoveTrack(const App::RequestType &req);
  typename App::ResponseType handleShufflePlaylist(const App::RequestType &req);
  typename App::ResponseType handleClearPlaylist(const App::RequestType &req);
  typename App::ResponseType handleExportPlaylist(const App::RequestType &req);
  typename App::ResponseType handleImportPlaylist(const App::RequestType &req);
  typename App::ResponseType
  handleCreateFromArtist(const App::RequestType &req);
  typename App::ResponseType handleCreateFromAlbum(const App::RequestType &req);
  typename App::ResponseType
  handleCreateFromSearch(const App::RequestType &req);
  typename App::ResponseType
  handleValidatePlaylists(const App::RequestType &req);
  typename App::ResponseType handleScanPlaylists(const App::RequestType &req);

  nlohmann::json playlistToJson(const Playlist &playlist,
                                const std::string &name);
  nlohmann::json playlistListToJson(const std::vector<std::string> &names);
  std::string getQueryParam(const App::RequestType &req, const std::string &key,
                            const std::string &defaultValue = "");
  int getQueryParamInt(const App::RequestType &req, const std::string &key,
                       int defaultValue = 0);
  bool getQueryParamBool(const App::RequestType &req, const std::string &key,
                         bool defaultValue = false);
};
