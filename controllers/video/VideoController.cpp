#include "VideoController.h"
#include "profilers/Profiler.h"
#include "services/video/FileSystemService.h"
#include "services/video/PlaybackStatus.h"
#include "services/video/StaticFileService.h"
#include "services/video/TrashHandler.h"
#include "services/video/VideoControlHandler.h"
#include "services/video/VideoThumbnailer.h"

std::string VideoController::activeSocket = "";

VideoController::VideoController(App &app, std::shared_ptr<Profiler> profiler)
    : RestController<App>(app), profiler(profiler) {}

void VideoController::init(std::shared_ptr<Profiler> profiler) {
  this->profiler = profiler;
}

void VideoController::register_all_routes() {
  app_.get("/", [this](const StringHttpRequest &req) -> StringHttpResponse {
    return handleGetIndex(req);
  });
  app_.get("/static/{filename}",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleServeStatic(req);
           });
  app_.post("/api/video/list",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleListFiles(req);
            });
  app_.post("/api/video/open",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleOpenVideo(req);
            });
  app_.post("/api/open-audio",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleOpenAudio(req);
            });
  app_.post("/api/trash",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleMoveToTrash(req);
            });
  app_.get("/api/video/status",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetPlaybackStatus(req);
           });
  app_.post("/api/mpv/control",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleControlMpv(req);
            });
  app_.post("/api/mpv/seek",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSeekMpv(req);
            });
  app_.post("/api/mpv/seek-fast",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleFastSeek(req);
            });
  app_.get("/api/mpv/property/{name}",
           [this](const StringHttpRequest &req) -> StringHttpResponse {
             return handleGetMpvProperty(req);
           });
  app_.post("/api/video/close",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleCloseVideo(req);
            });
  app_.post("/api/video/forceStop",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleForceStopVideo(req);
            });
  app_.post("/api/delete-directory",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleDeleteDirectory(req);
            });
  app_.post("/api/video/audio/track",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleSetAudioTrack(req);
            });
  app_.post("/api/video/thumbnail",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleExtractThumbnail(req);
            });
}

std::string
VideoController::getQueryParam(const StringHttpRequest &req,
                               const std::string &key,
                               const std::string &defaultValue) const {
  auto value = req.getQuery(key);
  return value.empty() ? defaultValue : value;
}

int VideoController::getQueryParamInt(const StringHttpRequest &req,
                                      const std::string &key,
                                      int defaultValue) const {
  auto value = req.getQuery(key);
  if (value.empty()) {
    return defaultValue;
  }
  try {
    return std::stoi(value);
  } catch (...) {
    return defaultValue;
  }
}

nlohmann::json
VideoController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}

std::string VideoController::urlDecode(const std::string &str) const {
  return StringHttpRequest::urlDecode(str);
}

nlohmann::json
VideoController::jsonValueToNlohmann(const Json::Value &value) const {
  nlohmann::json result;
  if (value.isNull()) {
    return result;
  }
  if (value.isBool()) {
    return value.asBool();
  }
  if (value.isInt()) {
    return value.asInt();
  }
  if (value.isDouble()) {
    return value.asDouble();
  }
  if (value.isString()) {
    return value.asString();
  }
  if (value.isArray()) {
    result = nlohmann::json::array();
    for (const auto &item : value) {
      result.push_back(jsonValueToNlohmann(item));
    }
    return result;
  }
  if (value.isObject()) {
    for (const auto &key : value.getMemberNames()) {
      result[key] = jsonValueToNlohmann(value[key]);
    }
    return result;
  }
  return result;
}

StringHttpResponse
VideoController::handleGetIndex(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (!profiler) {
      res.setHtmlContent("Profiler not initialized");
      res.setStatus(500);
      return res;
    }
    std::string indexContent =
        StaticFileService::getInstance().serveIndex(profiler->getIndexPath());
    res.setHtmlContent(indexContent);
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setHtmlContent(std::string("Error: ") + e.what());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleServeStatic(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string filename = req.getParam("filename");
    if (!profiler) {
      res.setHtmlContent("Profiler not initialized");
      res.setStatus(500);
      return res;
    }
    std::string fileContent = StaticFileService::getInstance().serveStaticFile(
        profiler->getDocumentRoot(), filename);
    std::string contentType =
        StaticFileService::getInstance().getContentType(filename);
    res.setBodyContent(fileContent);
    res.setHeader("Content-Type", contentType);
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setHtmlContent(std::string("Error: ") + e.what());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleListFiles(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto &fsService = FileSystemService::getInstance();
    std::string requestPath =
        profiler ? profiler->getVideoDirectory() : "/mnt/video";
    auto json = parseJsonBody(req);
    if (!json.is_null() && json.contains("path") && json["path"].is_string()) {
      requestPath = json["path"].get<std::string>();
    } else {
      std::string pathParam = getQueryParam(req, "path");
      if (!pathParam.empty()) {
        requestPath = urlDecode(pathParam);
      }
    }
    if (!fsService.isPathAllowed(requestPath)) {
      requestPath = profiler ? profiler->getVideoDirectory() : "/mnt/video";
    }
    if (!fsService.fileExists(requestPath) ||
        !fsService.isDirectory(requestPath)) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Directory not found: " + requestPath;
      res.setJsonContent(response.dump());
      res.setStatus(404);
      return res;
    }
    Json::Value jsonResult = fsService.listDirectory(requestPath);
    nlohmann::json result = jsonValueToNlohmann(jsonResult);
    res.setJsonContent(result.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleOpenVideo(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("path")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "No path provided";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    std::string path = json["path"].get<std::string>();
    Json::Value jsonResponse = VideoControlHandler::getInstance().handleOpen(
        path, activeSocket, false);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleOpenAudio(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("path")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "No path provided";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    std::string path = json["path"].get<std::string>();
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleOpen(path, activeSocket, true);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleMoveToTrash(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("path")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "No path provided";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    std::string path = json["path"].get<std::string>();
    Json::Value jsonResponse =
        TrashHandler::getInstance().handleMoveToTrash(path);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    if (response.contains("success") && !response["success"].get<bool>() &&
        response.contains("error")) {
      std::string error = response["error"].get<std::string>();
      if (error.find("Access denied") != std::string::npos) {
        res.setStatus(403);
      } else if (error.find("not found") != std::string::npos) {
        res.setStatus(404);
      } else if (error.find("Cannot delete") != std::string::npos) {
        res.setStatus(400);
      } else {
        res.setStatus(500);
      }
    } else {
      res.setStatus(200);
    }
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleGetPlaybackStatus(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    if (statusRequestInProgress.exchange(true)) {
      std::lock_guard<std::mutex> lock(statusMutex);
      if (statusCache.isValid) {
        res.setJsonContent(statusCache.data.dump());
        statusRequestInProgress = false;
        res.setStatus(200);
        return res;
      }
      statusRequestInProgress = false;
    }
    {
      std::lock_guard<std::mutex> lock(statusMutex);
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - statusCache.timestamp)
                         .count();
      if (statusCache.isValid && elapsed < 200) {
        res.setJsonContent(statusCache.data.dump());
        statusRequestInProgress = false;
        res.setStatus(200);
        return res;
      }
    }
    Json::Value jsonResponse =
        PlaybackStatus::getInstance().getStatus(activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    {
      std::lock_guard<std::mutex> lock(statusMutex);
      statusCache.data = response;
      statusCache.timestamp = std::chrono::steady_clock::now();
      statusCache.isValid = true;
    }
    res.setJsonContent(response.dump());
    statusRequestInProgress = false;
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
    statusRequestInProgress = false;
  }
  return res;
}

StringHttpResponse
VideoController::handleControlMpv(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("command")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Missing command parameter";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    std::string command = json["command"].get<std::string>();
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleControl(command, activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleSeekMpv(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null()) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Invalid JSON";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    if (!json.contains("time")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Missing 'time' parameter (seconds)";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    double seekTime = json["time"].get<double>();
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleSeek(seekTime, activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleFastSeek(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("time")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Missing 'time' parameter";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    double seekTime = json["time"].get<double>();
    if (seekTime < 0) {
      seekTime = 0;
    }
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleSeek(seekTime, activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleGetMpvProperty(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string propertyName = req.getParam("name");
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleGetProperty(propertyName,
                                                             activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleCloseVideo(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleClose(activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleForceStopVideo(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleForceStop(activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleDeleteDirectory(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("path")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "No path provided";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    std::string path = json["path"].get<std::string>();
    Json::Value jsonResponse =
        TrashHandler::getInstance().handleDeleteDirectory(path);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleSetAudioTrack(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("streamIndex")) {
      nlohmann::json response;
      response["success"] = false;
      response["error"] = "Missing streamIndex parameter";
      res.setJsonContent(response.dump());
      res.setStatus(400);
      return res;
    }
    int streamIndex = json["streamIndex"].get<int>();
    Json::Value jsonResponse =
        VideoControlHandler::getInstance().handleSetAudioTrack(streamIndex,
                                                               activeSocket);
    nlohmann::json response = jsonValueToNlohmann(jsonResponse);
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = e.what();
    res.setJsonContent(response.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
VideoController::handleExtractThumbnail(const StringHttpRequest &req) {
  StringHttpResponse res;

  return res;
}
