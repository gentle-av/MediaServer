#pragma once

#include "../../database/MusicDatabase.h"
#include "../../repositories/MusicRepository.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class AlbumArtController : public RestController<App> {
public:
  explicit AlbumArtController(App &app, std::shared_ptr<MusicDatabase> db,
                              MusicRepository &repo);
  ~AlbumArtController() = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleGetAlbumArt(const StringHttpRequest &req);
  StringHttpResponse handleGetAlbumArtByAlbum(const StringHttpRequest &req);
  StringHttpResponse handleUploadAlbumArt(const StringHttpRequest &req);
  StringHttpResponse handleDeleteAlbumArt(const StringHttpRequest &req);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;
  std::string detectMimeType(const std::vector<char> &data);
  StringHttpResponse createImageResponse(const std::vector<char> &artData,
                                         const StringHttpRequest &req);
  std::vector<char> base64Decode(const std::string &base64Str);
  std::shared_ptr<MusicDatabase> db;
  MusicRepository &musicRepository;
};
