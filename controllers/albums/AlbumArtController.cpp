#include "controllers/albums/AlbumArtController.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>

namespace fs = std::filesystem;

std::unique_ptr<AlbumArtService> AlbumArtController::albumArtService_ = nullptr;
AlbumArtWriter AlbumArtController::writer_;

AlbumArtController::AlbumArtController() {}

void AlbumArtController::init(std::unique_ptr<MusicDatabase> &db) {
  if (albumArtService_ == nullptr) {
    albumArtService_ = std::make_unique<AlbumArtService>(*db);
  }
}

void AlbumArtController::options(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
  resp->addHeader("Access-Control-Max-Age", "86400");
  callback(resp);
}

void AlbumArtController::getAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::string filePath = req->getParameter("path");
  if (filePath.empty()) {
    callback(drogon::HttpResponse::newNotFoundResponse());
    return;
  }
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  auto albumArt = albumArtService_->getAlbumArt(decodedPath);
  callback(albumArtService_->createImageResponse(albumArt));
}

void AlbumArtController::getAlbumArtByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  std::string decodedAlbum = drogon::utils::urlDecode(album);
  std::string artistFilter = req->getParameter("artist");
  auto albumArt =
      albumArtService_->getAlbumArtByAlbum(decodedAlbum, artistFilter);
  callback(albumArtService_->createImageResponse(albumArt));
}

void AlbumArtController::uploadAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->addHeader("Access-Control-Allow-Origin", "*");
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path") || !json->isMember("image_data")) {
    ResponseBuilder::sendError(callback,
                               "Missing 'path' or 'image_data' parameter");
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string imageBase64 = (*json)["image_data"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  if (!fs::exists(decodedPath)) {
    ResponseBuilder::sendError(callback, "File does not exist",
                               drogon::k404NotFound);
    return;
  }
  std::vector<char> imageData =
      drogon::utils::base64DecodeToVector(imageBase64);
  if (imageData.empty()) {
    ResponseBuilder::sendError(callback, "Failed to decode base64 image data");
    return;
  }
  if (!writer_.writeToFile(decodedPath, imageData)) {
    ResponseBuilder::sendError(callback, "Failed to write album art to file");
    return;
  }
  if (albumArtService_) {
    auto db = albumArtService_->getDatabase();
    if (db) {
      db->saveAlbumArt(decodedPath, imageData);
    }
  }
  Json::Value data;
  data["path"] = decodedPath;
  data["size"] = (int)imageData.size();
  data["status"] = "success";
  auto successResp = ResponseBuilder::success(data);
  successResp->addHeader("Access-Control-Allow-Origin", "*");
  callback(successResp);
}

void AlbumArtController::deleteAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("path")) {
    ResponseBuilder::sendError(callback, "Missing 'path' parameter");
    return;
  }
  std::string filePath = (*json)["path"].asString();
  std::string decodedPath = drogon::utils::urlDecode(filePath);
  if (!fs::exists(decodedPath)) {
    ResponseBuilder::sendError(callback, "File does not exist",
                               drogon::k404NotFound);
    return;
  }
  if (!writer_.removeFromFile(decodedPath)) {
    ResponseBuilder::sendError(callback,
                               "Failed to remove album art from file");
    return;
  }
  if (albumArtService_) {
    auto db = albumArtService_->getDatabase();
    if (db) {
      db->removeAlbumArt(decodedPath);
    }
  }
  Json::Value data;
  data["path"] = decodedPath;
  ResponseBuilder::sendSuccess(callback, data);
}
