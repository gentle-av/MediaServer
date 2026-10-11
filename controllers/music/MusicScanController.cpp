#include "MusicScanController.h"

MusicScanController::MusicScanController(App &app, MusicRepository &repo,
                                         PlaylistRepository &playlistRepo,
                                         const std::string &musicDir)
    : RestController<App>(app), musicRepository(repo),
      playlistRepository(playlistRepo), musicDirectory(musicDir) {}

nlohmann::json
MusicScanController::handleForceRescan(const StringHttpRequest &req,
                                       const std::string &targetDir) {
  nlohmann::json response = this->success_response("Force rescan started");
  response["scanning"] = true;
  response["directory"] = targetDir;
  return response;
}

nlohmann::json
MusicScanController::handleRemoveMissing(const StringHttpRequest &req) {
  StringHttpResponse res;
  if (musicRepository.isScanning()) {
    auto error = this->error_response(409, "Scan already in progress");
    res.setStatus(409);
    res.setJsonContent(error.dump());
    return error;
  }
  std::string targetDir = musicDirectory;
  auto queryDir = req.getQuery("dir");
  if (!queryDir.empty()) {
    targetDir = queryDir;
  }
  musicRepository.scanMusicDirectoryAsync(targetDir,
                                          [](int total, int processed) {});
  return this->handleForceRescan(req, targetDir);
}

nlohmann::json
MusicScanController::handleStartScan(const StringHttpRequest &req) {
  if (musicRepository.isScanning()) {
    auto resp = this->error_response(409, "Scan already in progress");
    resp["scanning"] = true;
    return resp;
  }
  std::string targetDir = musicDirectory;
  auto queryDir = req.getQuery("dir");
  if (!queryDir.empty()) {
    targetDir = queryDir;
  } else {
    auto body = req.getBodyString();
    if (!body.empty()) {
      try {
        auto json = nlohmann::json::parse(body);
        if (json.contains("dir") && json["dir"].is_string()) {
          targetDir = json["dir"].get<std::string>();
        }
      } catch (...) {
      }
    }
  }
  musicRepository.scanMusicDirectoryAsync(targetDir,
                                          [](int total, int processed) {});
  nlohmann::json resp = this->success_response("Scan started");
  resp["scanning"] = true;
  resp["directory"] = targetDir;
  return resp;
}

nlohmann::json
MusicScanController::handleScanStatus(const StringHttpRequest &req) {
  nlohmann::json resp;
  resp["success"] = true;
  resp["scanning"] = musicRepository.isScanning();
  return resp;
}

void MusicScanController::register_all_routes() {
  this->app_.post(
      "/api/music/remove-missing",
      [this](const StringHttpRequest &req) -> StringHttpResponse {
        StringHttpResponse res;
        auto result = handleRemoveMissing(req);
        if (result.contains("scanning") && !result["scanning"].get<bool>()) {
          res.setStatus(409);
          res.setJsonContent(result.dump());
          return res;
        }
        auto response = this->success_response("Force rescan started");
        response["scanning"] = true;
        std::string targetDir = musicDirectory;
        auto queryDir = req.getQuery("dir");
        if (!queryDir.empty()) {
          targetDir = queryDir;
        }
        response["directory"] = targetDir;
        res.setJsonContent(response.dump());
        res.setStatus(200);
        return res;
      });
  this->app_.post("/api/music/validate-playlists",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    StringHttpResponse res;
                    try {
                      int totalPlaylists =
                          playlistRepository.getAllPlaylistNames().size();
                      playlistRepository.validateAllPlaylists();
                      nlohmann::json response =
                          this->success_response("Playlists validated");
                      response["total_playlists"] = totalPlaylists;
                      res.setJsonContent(response.dump());
                      res.setStatus(200);
                    } catch (const std::exception &e) {
                      auto error = this->error_response(500, e.what());
                      res.setStatus(500);
                      res.setJsonContent(error.dump());
                    }
                    return res;
                  });
  this->app_.post("/api/music/scan",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    StringHttpResponse res;
                    nlohmann::json result = handleStartScan(req);
                    res.setJsonContent(result.dump());
                    res.setStatus(result.value("success", false) ? 200 : 409);
                    return res;
                  });
  this->app_.get("/api/music/scan/status",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   StringHttpResponse res;
                   nlohmann::json result = handleScanStatus(req);
                   res.setJsonContent(result.dump());
                   res.setStatus(200);
                   return res;
                 });
}
