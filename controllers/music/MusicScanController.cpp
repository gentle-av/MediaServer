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
}
