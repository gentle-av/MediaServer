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
  this->app_.post("/api/music/albumart/by-album",
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
AlbumArtController::handleGetAlbumArtByAlbum(const StringHttpRequest &req) {
  StringHttpResponse res;
  auto json = parseJsonBody(req);
  std::cout << "\n[DEBUG] === New Album Art Request ===" << std::endl;
  if (json.is_null()) {
    std::cout << "[DEBUG] Error: JSON body is null or invalid" << std::endl;
    res.setStatus(400);
    res.setJsonContent("{\"error\":\"Invalid JSON\"}");
    return res;
  }
  if (!json.contains("album")) {
    std::cout << "[DEBUG] Error: JSON missing 'album' field" << std::endl;
    res.setStatus(400);
    res.setJsonContent("{\"error\":\"Missing album field\"}");
    return res;
  }
  std::string album = json["album"].get<std::string>();
  std::string artistFilter =
      json.contains("artist") ? json["artist"].get<std::string>() : "";
  std::cout << "[DEBUG] Requested Album: '" << album << "'" << std::endl;
  std::cout << "[DEBUG] Requested Artist: '" << artistFilter << "'"
            << std::endl;
  if (!db) {
    std::cout << "[DEBUG] Error: MusicDatabase pointer is NULL!" << std::endl;
    res.setStatus(500);
    res.setJsonContent("{\"error\":\"Database not initialized\"}");
    return res;
  }
  std::string filePath = db->getFilePathByAlbumRaw(album, artistFilter);
  std::cout << "[DEBUG] getFilePathByAlbumRaw returned path: '" << filePath
            << "'" << std::endl;
  if (!filePath.empty()) {
    auto albumArt = db->getAlbumArt(filePath);
    std::cout << "[DEBUG] getAlbumArt size for direct path: "
              << albumArt.data.size() << " bytes" << std::endl;
    if (!albumArt.data.empty()) {
      std::cout << "[DEBUG] Success: Sending direct album art" << std::endl;
      return createImageResponse(albumArt.data);
    }
  }
  std::cout << "[DEBUG] Falling back to getTracksByAlbumRaw..." << std::endl;
  auto tracks = db->getTracksByAlbumRaw(album, artistFilter);
  std::cout << "[DEBUG] Found " << tracks.size() << " tracks for this album"
            << std::endl;
  for (const auto &track : tracks) {
    std::cout << "[DEBUG] Checking track path: '" << track.filePath << "'"
              << std::endl;
    auto albumArt = db->getAlbumArt(track.filePath);
    std::cout << "[DEBUG] Track art size: " << albumArt.data.size() << " bytes"
              << std::endl;
    if (!albumArt.data.empty()) {
      std::cout << "[DEBUG] Success: Sending album art from track" << std::endl;
      return createImageResponse(albumArt.data);
    }
  }
  std::cout
      << "[DEBUG] Error: No art data found in DB for this album. Sending 404"
      << std::endl;
  res.setStatus(404);
  res.setJsonContent("{\"error\":\"Album art not found\"}");
  return res;
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

StringHttpResponse
AlbumArtController::createImageResponse(const std::vector<char> &artData) {
  StringHttpResponse res;
  if (artData.empty()) {
    res.setStatus(404);
    return res;
  }
  res.setHeader("Content-Type", detectMimeType(artData));
  res.setBodyContent(std::string(artData.begin(), artData.end()));
  res.setStatus(200);
  return res;
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
