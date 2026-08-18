#pragma once

#include "../../repositories/MusicRepository.h"
#include "../../services/music/MetadataCache.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <nlohmann/json.hpp>

class MusicLibraryController : public RestController<App> {
public:
  explicit MusicLibraryController(
      App &app, MusicRepository &repo,
      std::shared_ptr<MetadataCache> cache = nullptr);
  ~MusicLibraryController() = default;

protected:
  void register_all_routes() override;

private:
  MusicRepository &musicRepository;
  std::shared_ptr<MetadataCache> metadataCache;

  typename App::ResponseType
  handleGetTracksByArtist(const App::RequestType &req,
                          const std::string &artist);
  typename App::ResponseType handleGetTracksByAlbum(const App::RequestType &req,
                                                    const std::string &album);
  typename App::ResponseType handleListFiles(const App::RequestType &req);
  typename App::ResponseType handleGetArtists(const App::RequestType &req);
  typename App::ResponseType handleGetAlbums(const App::RequestType &req);
  typename App::ResponseType
  handleGetAlbumsPaginated(const App::RequestType &req);

  nlohmann::json trackToJson(const MusicMetadata &track);
  nlohmann::json buildTrackResponse(const std::vector<MusicMetadata> &tracks);
  std::string getQueryParam(const App::RequestType &req, const std::string &key,
                            const std::string &defaultValue = "");
  int getQueryParamInt(const App::RequestType &req, const std::string &key,
                       int defaultValue = 1);
};
