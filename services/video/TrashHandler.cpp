#include "TrashHandler.h"
#include "FileSystemService.h"

TrashHandler &TrashHandler::getInstance() {
  static TrashHandler instance;
  return instance;
}

Json::Value TrashHandler::handleMoveToTrash(const std::string &path) {
  auto &fsService = FileSystemService::getInstance();
  Json::Value response;
  if (path.empty()) {
    response["success"] = false;
    response["error"] = "Empty path";
    return response;
  }
  if (!fsService.isPathAllowed(path)) {
    response["success"] = false;
    response["error"] =
        "Access denied: path must be under /mnt/video or /mnt/media";
    return response;
  }
  if (!fsService.fileExists(path)) {
    response["success"] = false;
    response["error"] = "File not found: " + path;
    return response;
  }
  if (fsService.isDirectory(path)) {
    response["success"] = false;
    response["error"] = "Cannot delete directory using this endpoint, use "
                        "/api/delete-directory";
    return response;
  }
  bool result = fsService.moveToTrash(path);
  if (result) {
    response["success"] = true;
    response["message"] = "File moved to trash";
  } else {
    response["success"] = false;
    response["error"] = "Failed to move file to trash";
  }
  return response;
}

Json::Value TrashHandler::handleDeleteDirectory(const std::string &path) {
  auto &fsService = FileSystemService::getInstance();
  Json::Value response;
  if (!fsService.isPathAllowed(path)) {
    response["success"] = false;
    response["error"] = "Access denied";
    return response;
  }
  if (!fsService.fileExists(path)) {
    response["success"] = false;
    response["error"] = "Path not found";
    return response;
  }
  if (!fsService.isDirectory(path)) {
    response["success"] = false;
    response["error"] = "Not a directory";
    return response;
  }
  bool result = fsService.deleteDirectory(path);
  response["success"] = result;
  response["message"] =
      result ? "Directory moved to trash" : "Failed to move directory";
  response["path"] = path;
  return response;
}
