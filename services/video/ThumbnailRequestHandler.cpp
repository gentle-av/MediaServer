#include "ThumbnailRequestHandler.h"
#include "FileSystemService.h"
#include "ThumbnailService.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

ThumbnailRequestHandler &ThumbnailRequestHandler::getInstance() {
  static ThumbnailRequestHandler instance;
  return instance;
}

Json::Value ThumbnailRequestHandler::buildErrorResponse(
    const std::string &error, bool useIcon, const std::string &ext) {
  Json::Value response;
  response["success"] = false;
  response["error"] = error;
  if (useIcon)
    response["use_icon"] = true;
  if (!ext.empty())
    response["extension"] = ext;
  return response;
}

Json::Value ThumbnailRequestHandler::buildSuccessResponse(
    const std::string &base64Thumbnail, int width, int quality,
    const std::string &path) {
  Json::Value response;
  response["success"] = true;
  response["thumbnail"] = "data:image/jpeg;base64," + base64Thumbnail;
  response["width"] = width;
  response["quality"] = quality;
  response["path"] = path;
  return response;
}

Json::Value
ThumbnailRequestHandler::handleSingleRequest(const std::string &videoPath,
                                             int width, int quality) {
  auto &thumbnailService = ThumbnailService::getInstance();
  auto &fsService = FileSystemService::getInstance();
  if (videoPath.empty())
    return buildErrorResponse("No path parameter provided");
  if (!fsService.isPathAllowed(videoPath))
    return buildErrorResponse(
        "Access denied: path outside allowed directories");
  if (!fsService.fileExists(videoPath))
    return buildErrorResponse("File not found: " + videoPath);
  std::string ext = fs::path(videoPath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (!fsService.isVideoFile(ext))
    return buildErrorResponse("Not a video file", true, ext);
  if (!thumbnailService.isVideoValid(videoPath))
    return buildErrorResponse("Video file is corrupted or invalid", true);
  try {
    std::string base64Thumbnail =
        thumbnailService.generateThumbnailBase64(videoPath, width, quality);
    if (base64Thumbnail.empty())
      return buildErrorResponse("Could not generate thumbnail", true);
    return buildSuccessResponse(base64Thumbnail, width, quality, videoPath);
  } catch (const std::exception &e) {
    return buildErrorResponse(
        std::string("Failed to generate thumbnail: ") + e.what(), true);
  }
}

Json::Value ThumbnailRequestHandler::handleBatchRequest(
    const std::vector<std::string> &paths, int width, int quality) {
  auto &thumbnailService = ThumbnailService::getInstance();
  auto &fsService = FileSystemService::getInstance();
  Json::Value results;
  results["thumbnails"] = Json::Value(Json::arrayValue);
  for (const auto &path : paths) {
    Json::Value item;
    item["path"] = path;
    if (!fsService.fileExists(path)) {
      item["success"] = false;
      item["error"] = "File not found";
    } else {
      std::string ext = fs::path(path).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (!fsService.isVideoFile(ext)) {
        item["success"] = false;
        item["error"] = "Not a video file";
      } else if (!thumbnailService.isVideoValid(path)) {
        item["success"] = false;
        item["error"] = "Video file is corrupted or invalid";
      } else {
        try {
          std::string base64Thumbnail =
              thumbnailService.generateThumbnailBase64(path, width, quality);
          if (!base64Thumbnail.empty()) {
            item["success"] = true;
            item["thumbnail"] = "data:image/jpeg;base64," + base64Thumbnail;
          } else {
            item["success"] = false;
            item["error"] = "Could not generate thumbnail";
          }
        } catch (const std::exception &e) {
          item["success"] = false;
          item["error"] = e.what();
        }
      }
    }
    results["thumbnails"].append(item);
  }
  results["success"] = true;
  results["count"] = static_cast<int>(paths.size());
  return results;
}
