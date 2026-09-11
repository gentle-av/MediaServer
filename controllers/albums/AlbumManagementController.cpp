#include "AlbumManagementController.h"
#include "../../services/video/FileSystemService.h"
#include <filesystem>

namespace fs = std::filesystem;

AlbumManagementController::AlbumManagementController(
    App &app, std::shared_ptr<MusicDatabase> db,
    std::shared_ptr<MetadataCache> cache, MusicRepository &repo)
    : RestController<App>(app), db(db), cache(cache), musicRepository(repo) {}

void AlbumManagementController::register_all_routes() {
  this->app_.post("/api/music/delete-album",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    return this->handleDeleteAlbum(req);
                  });
}

StringHttpResponse
AlbumManagementController::handleDeleteAlbum(const StringHttpRequest &req) {
  StringHttpResponse res;
  auto json = parseJsonBody(req);
  if (json.is_null() || !json.contains("album") || !json.contains("artist")) {
    res.setStatus(400);
    res.setJsonContent(
        this->error_response(400, "Missing album or artist parameter").dump());
    return res;
  }
  std::string albumName = json["album"].get<std::string>();
  std::string artistName = json["artist"].get<std::string>();
  try {
    auto tracks = db->getTracksByAlbumRaw(albumName, artistName);
    if (tracks.empty()) {
      res.setStatus(404);
      res.setJsonContent(this->error_response(404, "Album not found").dump());
      return res;
    }
    std::string albumFolderPath;
    for (const auto &track : tracks) {
      fs::path trackPath(track.filePath);
      albumFolderPath = trackPath.parent_path().string();
      break;
    }
    if (albumFolderPath.empty()) {
      res.setStatus(500);
      res.setJsonContent(
          this->error_response(500, "Could not determine album folder path")
              .dump());
      return res;
    }
    for (const auto &track : tracks) {
      db->removeFile(track.filePath);
      if (cache) {
        cache->erase(track.filePath);
      }
    }
    int deletedFiles = 0;
    int errorCount = 0;
    auto &fsService = FileSystemService::getInstance();
    if (fs::exists(albumFolderPath)) {
      if (fsService.moveToTrash(albumFolderPath)) {
        deletedFiles = static_cast<int>(tracks.size());
      } else {
        errorCount++;
      }
    } else {
      errorCount++;
    }
    std::string artistFolderPath =
        fs::path(albumFolderPath).parent_path().string();
    bool shouldDeleteArtistFolder = false;
    int artistFolderDeleted = 0;
    if (!artistFolderPath.empty() && fs::exists(artistFolderPath)) {
      auto remainingAlbums = db->getAlbumsRaw(artistName);
      bool hasOtherAlbums = false;
      for (const auto &[album, artist, year] : remainingAlbums) {
        if (artist == artistName) {
          hasOtherAlbums = true;
          break;
        }
      }
      if (!hasOtherAlbums) {
        bool hasOtherFiles = false;
        try {
          for (const auto &entry :
               fs::recursive_directory_iterator(artistFolderPath)) {
            if (fs::is_regular_file(entry.path())) {
              hasOtherFiles = true;
              break;
            }
          }
        } catch (...) {
        }
        if (!hasOtherFiles) {
          shouldDeleteArtistFolder = true;
        }
      }
    }
    if (shouldDeleteArtistFolder && fs::exists(artistFolderPath)) {
      if (fsService.moveToTrash(artistFolderPath)) {
        artistFolderDeleted = 1;
      }
    }
    musicRepository.invalidateAll();
    nlohmann::json responseData;
    responseData["success"] = true;
    responseData["deletedFiles"] = deletedFiles;
    responseData["errorCount"] = errorCount;
    responseData["album"] = albumName;
    responseData["artist"] = artistName;
    responseData["albumFolder"] = albumFolderPath;
    responseData["artistFolderDeleted"] = (artistFolderDeleted == 1);
    if (shouldDeleteArtistFolder) {
      responseData["artistFolder"] = artistFolderPath;
    }
    res.setStatus(200);
    res.setJsonContent(responseData.dump());
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

nlohmann::json
AlbumManagementController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}
