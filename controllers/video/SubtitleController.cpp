#include "SubtitleController.h"

SubtitleController::SubtitleController(
    App &app, std::shared_ptr<SubtitleService> subtitleService)
    : RestController<App>(app), subtitleService(subtitleService) {}

void SubtitleController::register_all_routes() {
  this->app_.get("/api/video/subtitles",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetSubtitleTracks(req);
                 });
  this->app_.post("/api/video/subtitles/set",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleSetSubtitleTrack(req);
                  });
  this->app_.get("/api/video/subtitles/current",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleGetCurrentSubtitleTrack(req);
                 });
  this->app_.post("/api/video/subtitles/disable",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleDisableSubtitles(req);
                  });
}

StringHttpResponse
SubtitleController::handleGetSubtitleTracks(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto result = subtitleService->getSubtitleTracks();
    if (!result.has_value()) {
      res.setStatus(400);
      res.setJsonContent(this->error_response(400, result.error()).dump());
      return res;
    }
    nlohmann::json tracksJson = nlohmann::json::array();
    for (const auto &track : result.value()) {
      nlohmann::json t;
      t["streamIndex"] = track.streamIndex;
      t["codec"] = track.codecName;
      t["language"] = track.language;
      t["title"] = track.title;
      t["isDefault"] = track.isDefault;
      t["isForced"] = track.isForced;
      tracksJson.push_back(t);
    }
    nlohmann::json response;
    response["success"] = true;
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(result.value().size());
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
SubtitleController::handleSetSubtitleTrack(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = parseJsonBody(req);
    if (json.is_null() || !json.contains("streamIndex")) {
      res.setStatus(400);
      res.setJsonContent(
          this->error_response(400, "Missing 'streamIndex' parameter").dump());
      return res;
    }
    int streamIndex = json["streamIndex"].get<int>();
    auto result = subtitleService->setSubtitleTrack(streamIndex);
    if (!result.has_value()) {
      res.setStatus(500);
      res.setJsonContent(this->error_response(500, result.error()).dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Subtitle track set successfully";
    response["streamIndex"] = streamIndex;
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse SubtitleController::handleGetCurrentSubtitleTrack(
    const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto result = subtitleService->getCurrentSubtitleTrack();
    if (!result.has_value()) {
      res.setStatus(500);
      res.setJsonContent(this->error_response(500, result.error()).dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    if (result.value().has_value()) {
      const auto &track = result.value().value();
      response["track"]["streamIndex"] = track.streamIndex;
      response["track"]["codec"] = track.codecName;
      response["track"]["language"] = track.language;
      response["track"]["title"] = track.title;
      response["track"]["isDefault"] = track.isDefault;
      response["track"]["isForced"] = track.isForced;
      response["hasTrack"] = true;
    } else {
      response["hasTrack"] = false;
    }
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
SubtitleController::handleDisableSubtitles(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto result = subtitleService->disableSubtitles();
    if (!result.has_value()) {
      res.setStatus(500);
      res.setJsonContent(this->error_response(500, result.error()).dump());
      return res;
    }
    nlohmann::json response;
    response["success"] = true;
    response["message"] = "Subtitles disabled successfully";
    res.setJsonContent(response.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

nlohmann::json
SubtitleController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}
