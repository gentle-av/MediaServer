#include "MusicScanController.h"
#include <filesystem>

MusicScanController::MusicScanController(App &app, MusicRepository &repo,
                                         const std::string &musicDir)
    : RestController<App>(app), musicRepository(repo),
      musicDirectory(musicDir) {}

void MusicScanController::register_all_routes() {
  this->app_.get("/api/music/force-rescan",
                 [this](const App::RequestType &req) -> App::ResponseType {
                   App::ResponseType res;

                   if (musicRepository.isScanning()) {
                     auto error =
                         this->error_response(409, "Scan already in progress");
                     res.setStatus(409);
                     res.setJsonContent(error.dump());
                     return res;
                   }

                   std::string musicDir = musicDirectory;
                   auto queryDir = req.getQuery("dir");
                   if (!queryDir.empty()) {
                     musicDir = queryDir;
                   }

                   musicRepository.scanMusicDirectoryAsync(
                       musicDir, [](int total, int processed) {});

                   nlohmann::json response =
                       this->success_response("Force rescan started");
                   response["scanning"] = true;
                   response["directory"] = musicDir;

                   res.setJsonContent(response.dump());
                   res.setStatus(200);
                   return res;
                 });
  this->app_.post(
      "/api/music/remove-missing",
      [this](const App::RequestType &req) -> App::ResponseType {
        App::ResponseType res;
        std::vector<std::string> removedTracks;
        std::vector<std::string> errors;
        if (musicRepository.isScanning()) {
          auto error = this->error_response(
              409, "Scan in progress, cannot remove missing tracks");
          res.setStatus(409);
          res.setJsonContent(error.dump());
          return res;
        }
        musicRepository.forEachTrack([&](const MusicMetadata &track) {
          try {
            if (!std::filesystem::exists(
                    std::filesystem::path(track.filePath))) {
              if (musicRepository.removeTrack(track.filePath)) {
                removedTracks.push_back(track.filePath);
              } else {
                errors.push_back("Failed to remove: " + track.filePath);
              }
            }
          } catch (const std::exception &e) {
            errors.push_back("Error checking " + track.filePath + ": " +
                             e.what());
          }
        });
        musicRepository.invalidateAll();
        nlohmann::json response =
            this->success_response("Missing tracks removal completed");
        response["removed_count"] = removedTracks.size();
        response["removed_tracks"] = removedTracks;
        response["errors"] = errors;
        res.setJsonContent(response.dump());
        res.setStatus(200);
        return res;
      });
}
