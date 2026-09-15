#include "AlbumArtController.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

AlbumArtController::AlbumArtController(App &app,
                                       std::shared_ptr<MusicDatabase> db,
                                       MusicRepository &repo)
    : RestController<App>(app), db(db), musicRepository(repo) {}

void AlbumArtController::register_all_routes() {
  this->app_.get("/api/music/albumart",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetAlbumArt(req);
                 });
  this->app_.get("/api/music/albumart/by-album",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetAlbumArtByAlbum(req);
                 });
  this->app_.post("/api/music/upload-album-art",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleUploadAlbumArt(req);
                  });
  this->app_.post("/api/music/albumart/delete",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleDeleteAlbumArt(req);
                  });
}

StringHttpResponse
AlbumArtController::handleGetAlbumArtByAlbum(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string album = this->getQueryParam(req, "album");
  std::string artistFilter = this->getQueryParam(req, "artist");
  if (album.empty()) {
    res.setStatus(400);
    res.setJsonContent("{\"error\":\"Missing album parameter\"}");
    return res;
  }
  if (!db) {
    res.setStatus(500);
    res.setJsonContent("{\"error\":\"Database not initialized\"}");
    return res;
  }
  std::string filePath = db->getFilePathByAlbumRaw(album, artistFilter);
  if (!filePath.empty()) {
    auto albumArt = db->getAlbumArt(filePath);
    if (!albumArt.data.empty()) {
      return createImageResponse(albumArt.data);
    }
  }
  auto tracks = db->getTracksByAlbumRaw(album, artistFilter);
  for (const auto &track : tracks) {
    auto albumArt = db->getAlbumArt(track.filePath);
    if (!albumArt.data.empty()) {
      return createImageResponse(albumArt.data);
    }
  }
  res.setStatus(404);
  res.setJsonContent("{\"error\":\"Album art not found\"}");
  return res;
}

StringHttpResponse
AlbumArtController::handleGetAlbumArt(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string filePath = this->getQueryParam(req, "path");
  if (filePath.empty()) {
    res.setStatus(404);
    return res;
  }
  auto albumArt = db->getAlbumArt(filePath);
  return createImageResponse(albumArt.data);
}

StringHttpResponse
AlbumArtController::handleUploadAlbumArt(const StringHttpRequest &req) {
  StringHttpResponse res;
  auto json = parseJsonBody(req);
  if (json.is_null() || !json.contains("path") ||
      !json.contains("image_data")) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Missing 'path' or 'image_data' parameter")
            .dump());
    return res;
  }
  std::string filePath = json["path"].get<std::string>();
  std::string imageBase64 = json["image_data"].get<std::string>();
  if (!fs::exists(filePath)) {
    res.setStatus(404);
    res.setJsonContent(this->error_response(404, "File does not exist").dump());
    return res;
  }
  std::vector<char> imageData = base64Decode(imageBase64);
  if (imageData.empty()) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Failed to decode base64 image data").dump());
    return res;
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    res.setStatus(500);
    res.setJsonContent(
        this->error_response(500, "Failed to write album art to file").dump());
    return res;
  }
  file.write(imageData.data(), imageData.size());
  file.close();
  db->saveAlbumArt(filePath, imageData);
  musicRepository.invalidateAll();
  nlohmann::json responseData;
  responseData["success"] = true;
  responseData["path"] = filePath;
  responseData["size"] = static_cast<int>(imageData.size());
  responseData["status"] = "success";
  res.setStatus(200);
  res.setJsonContent(responseData.dump());
  return res;
}

StringHttpResponse
AlbumArtController::handleDeleteAlbumArt(const StringHttpRequest &req) {
  StringHttpResponse res;
  auto json = parseJsonBody(req);
  if (json.is_null() || !json.contains("path")) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Missing 'path' parameter").dump());
    return res;
  }
  std::string filePath = json["path"].get<std::string>();
  if (!fs::exists(filePath)) {
    res.setStatus(404);
    res.setJsonContent(this->error_response(404, "File does not exist").dump());
    return res;
  }
  try {
    fs::remove(filePath);
  } catch (...) {
    res.setStatus(500);
    res.setJsonContent(
        this->error_response(500, "Failed to remove album art from file")
            .dump());
    return res;
  }
  db->removeAlbumArt(filePath);
  musicRepository.invalidateAll();
  nlohmann::json responseData;
  responseData["success"] = true;
  responseData["path"] = filePath;
  res.setStatus(200);
  res.setJsonContent(responseData.dump());
  return res;
}

StringHttpResponse
AlbumArtController::createImageResponse(const std::vector<char> &artData) {
  StringHttpResponse res;
  if (artData.empty()) {
    res.setStatus(404);
    return res;
  }
  static const char b64Chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string b64Str;
  b64Str.reserve(((artData.size() + 2) / 3) * 4);
  int val = 0;
  int valb = -6;
  for (unsigned char c : artData) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      b64Str.push_back(b64Chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    b64Str.push_back(b64Chars[((val << (8 - (valb + 8))) >> 2) & 0x3F]);
  }
  while (b64Str.size() % 4) {
    b64Str.push_back('=');
  }
  nlohmann::json jsonResponse;
  jsonResponse["success"] = true;
  jsonResponse["mimeType"] = detectMimeType(artData);
  jsonResponse["imageData"] = b64Str;
  res.setHeader("Content-Type", "application/json; charset=utf-8");
  res.setBodyContent(jsonResponse.dump());
  res.setStatus(200);
  return res;
}

std::string AlbumArtController::detectMimeType(const std::vector<char> &data) {
  if (data.size() >= 4) {
    if (data[0] == (char)0xFF && data[1] == (char)0xD8)
      return "image/jpeg";
    if (data[0] == (char)0x89 && data[1] == (char)0x50)
      return "image/png";
    if (data[0] == (char)0x47 && data[1] == (char)0x49)
      return "image/gif";
  }
  return "application/octet-stream";
}

std::string
AlbumArtController::getQueryParam(const StringHttpRequest &req,
                                  const std::string &key,
                                  const std::string &defaultValue) const {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

nlohmann::json
AlbumArtController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}

std::vector<char>
AlbumArtController::base64Decode(const std::string &base64Str) {
  static const std::string b64Chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> b64Table(256, -1);
  for (int i = 0; i < 64; i++)
    b64Table[b64Chars[i]] = i;
  std::vector<char> decodedData;
  int bitBuffer = 0;
  int bitCount = -8;
  for (unsigned char currentChar : base64Str) {
    if (b64Table[currentChar] == -1)
      continue;
    bitBuffer = (bitBuffer << 6) + b64Table[currentChar];
    bitCount += 6;
    if (bitCount >= 0) {
      decodedData.push_back(char((bitBuffer >> bitCount) & 0xFF));
      bitCount -= 8;
    }
  }
  return decodedData;
}
