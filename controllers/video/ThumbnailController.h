#pragma once

#include "../../database/ImageDatabase.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpRequest.h>
#include <html-server/templates/HttpResponse.h>
#include <nlohmann/json.hpp>

class ThumbnailController : public RestController<App> {
public:
  explicit ThumbnailController(App &app, ImageDatabase &database);
  ~ThumbnailController() override = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleGetThumbnail(const StringHttpRequest &req);
  StringHttpResponse handleGetThumbnailByPath(const StringHttpRequest &req);
  StringHttpResponse handleGetThumbnailList(const StringHttpRequest &req);
  StringHttpResponse handleDeleteThumbnail(const StringHttpRequest &req);
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;

  ImageDatabase &database;
};
