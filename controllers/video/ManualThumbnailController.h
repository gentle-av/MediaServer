#pragma once

#include "database/ImageDatabase.h"
#include "services/video/ManualThumbnailUpdater.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpRequest.h>
#include <html-server/templates/HttpResponse.h>
#include <nlohmann/json.hpp>

class ManualThumbnailController : public RestController<App> {
public:
  ManualThumbnailController(App &app, ImageDatabase &database);
  ~ManualThumbnailController() override = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleUpdateThumbnail(const StringHttpRequest &req);
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;

  ImageDatabase &database;
  ManualThumbnailUpdater updater;
};
