#include "PlayerController.h"
#include <nlohmann/json.hpp>

PlayerController::PlayerController(
    App &app, std::shared_ptr<AudioPlaybackService> playbackService,
    std::shared_ptr<AudioOutputService> outputService)
    : RestController<App>(app), playbackService(playbackService),
      outputService(outputService) {}

void PlayerController::register_all_routes() {
  app_.post("/api/audio/play", [this](const StringHttpRequest &) {
    playbackService->togglePause(false);
    StringHttpResponse res;
    res.setJsonContent("{\"success\":true}");
    res.setStatus(200);
    return res;
  });
  app_.post("/api/audio/pause", [this](const StringHttpRequest &) {
    playbackService->togglePause(true);
    StringHttpResponse res;
    res.setJsonContent("{\"success\":true}");
    res.setStatus(200);
    return res;
  });
  app_.post("/api/audio/stop", [this](const StringHttpRequest &) {
    playbackService->stop();
    StringHttpResponse res;
    res.setJsonContent("{\"success\":true}");
    res.setStatus(200);
    return res;
  });
  app_.post("/api/audio/seek", [this](const StringHttpRequest &req) {
    StringHttpResponse res;
    try {
      auto json = nlohmann::json::parse(req.getBodyString());
      double position = json["position"].get<double>();
      playbackService->seek(position);
      res.setJsonContent("{\"success\":true}");
      res.setStatus(200);
    } catch (...) {
      res.setJsonContent("{\"success\":false,\"error\":\"Invalid JSON\"}");
      res.setStatus(400);
    }
    return res;
  });
  app_.post("/api/audio/playlist", [this](const StringHttpRequest &req) {
    StringHttpResponse res;
    try {
      auto json = nlohmann::json::parse(req.getBodyString());
      std::vector<std::string> tracks;
      for (const auto &track : json["tracks"]) {
        tracks.push_back(
            StringHttpRequest::urlDecode(track.get<std::string>()));
      }
      playbackService->setPlaylist(std::move(tracks));
      res.setJsonContent("{\"success\":true}");
      res.setStatus(200);
    } catch (...) {
      res.setJsonContent("{\"success\":false,\"error\":\"Invalid JSON\"}");
      res.setStatus(400);
    }
    return res;
  });
  app_.post("/api/audio/file", [this](const StringHttpRequest &req) {
    StringHttpResponse res;
    try {
      auto json = nlohmann::json::parse(req.getBodyString());
      std::string path = json["path"].get<std::string>();
      playbackService->setPlaylist({path});
      res.setJsonContent("{\"success\":true}");
      res.setStatus(200);
    } catch (...) {
      res.setJsonContent("{\"success\":false,\"error\":\"Invalid JSON\"}");
      res.setStatus(400);
    }
    return res;
  });
  app_.get("/api/audio/state", [this](const StringHttpRequest &) {
    StringHttpResponse res;
    nlohmann::json state = playbackService->getPlaybackState();
    nlohmann::json wrapped;
    wrapped["success"] = true;
    wrapped["data"] = state;
    res.setJsonContent(wrapped.dump());
    res.setStatus(200);
    return res;
  });
  app_.get("/api/audio/time", [this](const StringHttpRequest &) {
    StringHttpResponse res;
    nlohmann::json timeInfo = playbackService->getTimeInfo();
    nlohmann::json wrapped;
    wrapped["success"] = true;
    wrapped["data"] = timeInfo;
    res.setJsonContent(wrapped.dump());
    res.setStatus(200);
    return res;
  });
  app_.get("/api/audio/volume", [this](const StringHttpRequest &) {
    StringHttpResponse res;
    nlohmann::json wrapped;
    wrapped["success"] = true;
    wrapped["data"]["volume"] = outputService->getVolume();
    res.setJsonContent(wrapped.dump());
    res.setStatus(200);
    return res;
  });
  app_.post("/api/audio/volume", [this](const StringHttpRequest &req) {
    StringHttpResponse res;
    try {
      auto json = nlohmann::json::parse(req.getBodyString());
      outputService->setVolume(json["volume"].get<int>());
      res.setJsonContent("{\"success\":true}");
      res.setStatus(200);
    } catch (...) {
      res.setStatus(400);
    }
    return res;
  });
  app_.post("/api/audio/mute", [this](const StringHttpRequest &) {
    outputService->toggleMute();
    StringHttpResponse res;
    nlohmann::json wrapped;
    wrapped["success"] = true;
    wrapped["data"]["muted"] = outputService->isMuted();
    res.setJsonContent(wrapped.dump());
    res.setStatus(200);
    return res;
  });
  app_.get("/api/audio/output", [this](const StringHttpRequest &) {
    StringHttpResponse res;
    nlohmann::json wrapped;
    wrapped["success"] = true;
    wrapped["data"]["current"] = outputService->getCurrentOutput();
    wrapped["data"]["available"] = outputService->getAvailableOutputs();
    res.setJsonContent(wrapped.dump());
    res.setStatus(200);
    return res;
  });
}
