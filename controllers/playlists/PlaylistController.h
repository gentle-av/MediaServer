#pragma once
#include "../../repositories/MusicRepository.h"
#include "../../repositories/PlaylistRepository.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
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

  StringHttpResponse handleGetPlaylists(const StringHttpRequest &req);
  StringHttpResponse handleGetPlaylist(const StringHttpRequest &req);
  StringHttpResponse handleCreatePlaylist(const StringHttpRequest &req);
  StringHttpResponse handleUpdatePlaylist(const StringHttpRequest &req);
  StringHttpResponse handleDeletePlaylist(const StringHttpRequest &req);
  StringHttpResponse handleRenamePlaylist(const StringHttpRequest &req);
  StringHttpResponse handleAddTracks(const StringHttpRequest &req);
  StringHttpResponse handleRemoveTrack(const StringHttpRequest &req);
  StringHttpResponse handleShufflePlaylist(const StringHttpRequest &req);
  StringHttpResponse handleClearPlaylist(const StringHttpRequest &req);
  StringHttpResponse handleExportPlaylist(const StringHttpRequest &req);
  StringHttpResponse handleImportPlaylist(const StringHttpRequest &req);
  StringHttpResponse handleCreateFromArtist(const StringHttpRequest &req);
  StringHttpResponse handleCreateFromAlbum(const StringHttpRequest &req);
  StringHttpResponse handleCreateFromSearch(const StringHttpRequest &req);
  StringHttpResponse handleValidatePlaylists(const StringHttpRequest &req);
  StringHttpResponse handleScanPlaylists(const StringHttpRequest &req);

  nlohmann::json playlistToJson(const Playlist &playlist,
                                const std::string &name);
  nlohmann::json playlistListToJson(const std::vector<std::string> &names);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "");
  int getQueryParamInt(const StringHttpRequest &req, const std::string &key,
                       int defaultValue = 0);
  bool getQueryParamBool(const StringHttpRequest &req, const std::string &key,
                         bool defaultValue = false);
};
