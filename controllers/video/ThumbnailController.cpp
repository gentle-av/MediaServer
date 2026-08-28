#include "ThumbnailController.h"
#include <cppcodec/base64_rfc4648.hpp>
#include <filesystem>

ThumbnailController::ThumbnailController(App &app, ImageDatabase &database)
    : RestController<App>(app), database(database) {}

void ThumbnailController::register_all_routes() {
  app_.get("/api/thumbnails",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetThumbnail(req);
           });
  app_.get("/api/thumbnails/list",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetThumbnailList(req);
           });
  app_.del("/api/thumbnails",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleDeleteThumbnail(req);
           });
}

StringHttpResponse
ThumbnailController::handleGetThumbnail(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string filePath = getQueryParam(req, "path");
  if (filePath.empty()) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = "Missing path parameter";
    res.setJsonContent(error.dump());
    res.setStatus(400);
    return res;
  }
  if (!std::filesystem::exists(filePath)) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = "File not found";
    res.setJsonContent(error.dump());
    res.setStatus(404);
    return res;
  }
  auto imageOpt = database.getImage(filePath);
  if (!imageOpt.has_value()) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = "Thumbnail not found";
    res.setJsonContent(error.dump());
    res.setStatus(404);
    return res;
  }
  std::string imageData(reinterpret_cast<const char *>(imageOpt->data.data()),
                        imageOpt->data.size());
  res.setBodyContent(imageData);
  res.setHeader("Content-Type", imageOpt->mimeType);
  res.setHeader("Cache-Control", "public, max-age=31536000");
  res.setStatus(200);
  return res;
}

StringHttpResponse
ThumbnailController::handleGetThumbnailList(const StringHttpRequest &req) {
  StringHttpResponse res;
  auto paths = database.getAllPaths();
  nlohmann::json response;
  response["success"] = true;
  response["count"] = static_cast<int>(paths.size());
  nlohmann::json items = nlohmann::json::array();
  for (const auto &path : paths) {
    nlohmann::json item;
    item["path"] = path;
    auto imageOpt = database.getImage(path);
    if (imageOpt.has_value()) {
      std::string base64Data = cppcodec::base64_rfc4648::encode(
          imageOpt->data.data(), imageOpt->data.size());
      item["mimeType"] = imageOpt->mimeType;
      item["size"] = static_cast<int>(imageOpt->data.size());
      item["data"] = base64Data;
    }
    items.push_back(item);
  }
  response["thumbnails"] = items;
  res.setJsonContent(response.dump());
  res.setStatus(200);
  return res;
}

StringHttpResponse
ThumbnailController::handleDeleteThumbnail(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string filePath = getQueryParam(req, "path");
  if (filePath.empty()) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = "Missing path parameter";
    res.setJsonContent(error.dump());
    res.setStatus(400);
    return res;
  }
  if (!database.imageExists(filePath)) {
    nlohmann::json error;
    error["success"] = false;
    error["error"] = "Thumbnail not found";
    res.setJsonContent(error.dump());
    res.setStatus(404);
    return res;
  }
  bool success = database.removeImage(filePath);
  nlohmann::json response;
  response["success"] = success;
  response["message"] =
      success ? "Thumbnail deleted" : "Failed to delete thumbnail";
  res.setJsonContent(response.dump());
  res.setStatus(success ? 200 : 500);
  return res;
}

std::string
ThumbnailController::getQueryParam(const StringHttpRequest &req,
                                   const std::string &key,
                                   const std::string &defaultValue) const {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}
