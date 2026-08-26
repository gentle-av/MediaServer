#pragma once

#include "../../repositories/MusicRepository.h"
#include "../../services/music/MetadataCache.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
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

  StringHttpResponse handleGetTracksByArtist(const StringHttpRequest &req,
                                             const std::string &artist);
  StringHttpResponse handleGetTracksByAlbum(const StringHttpRequest &req,
                                            const std::string &album);
  StringHttpResponse handleListFiles(const StringHttpRequest &req);
  StringHttpResponse handleGetArtists(const StringHttpRequest &req);
  StringHttpResponse handleGetAlbums(const StringHttpRequest &req);
  StringHttpResponse handleGetAlbumsPaginated(const StringHttpRequest &req);

  nlohmann::json trackToJson(const MusicMetadata &track);
  nlohmann::json buildTrackResponse(const std::vector<MusicMetadata> &tracks);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "");
  int getQueryParamInt(const StringHttpRequest &req, const std::string &key,
                       int defaultValue = 1);
};
