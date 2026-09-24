#include "AlbumManagementController.h"
#include "../../services/video/FileSystemService.h"
#include <filesystem>

namespace fs = std::filesystem;

StringHttpResponse
AlbumManagementController::handleDeleteAlbum(const StringHttpRequest &req) {
  StringHttpResponse response;
  auto jsonBody = parseJsonBody(req);
  if (jsonBody.is_null() || !jsonBody.contains("album") ||
      !jsonBody.contains("artist")) {
    response.setStatus(400);
    response.setJsonContent(
        this->error_response(400, "Missing parameters").dump());
    return response;
  }
  std::string targetAlbum = jsonBody["album"].get<std::string>();
  std::string targetArtist = jsonBody["artist"].get<std::string>();
  try {
    auto albumTracks = db->getTracksByAlbumRaw(targetAlbum, targetArtist);
    if (albumTracks.empty()) {
      response.setStatus(404);
      response.setJsonContent(
          this->error_response(404, "Album not found").dump());
      return response;
    }
    std::string folderPath;
    for (const auto &track : albumTracks) {
      fs::path fileSystemPath(track.filePath);
      folderPath = fileSystemPath.parent_path().string();
      break;
    }
    if (folderPath.empty()) {
      response.setStatus(500);
      response.setJsonContent(
          this->error_response(500, "Path not resolved").dump());
      return response;
    }
    auto &fsService = FileSystemService::getInstance();
    bool folderExisted = fs::exists(folderPath);
    bool folderRemoved = true;
    if (folderExisted) {
      folderRemoved = fsService.moveToTrash(folderPath);
    }
    if (!folderRemoved) {
      response.setStatus(500);
      response.setJsonContent(
          this->error_response(500, "Failed to move album folder to trash")
              .dump());
      return response;
    }
    int countDeleted = 0;
    for (const auto &track : albumTracks) {
      if (db->removeFile(track.filePath)) {
        countDeleted++;
      }
      if (cache) {
        cache->erase(track.filePath);
      }
    }
    std::string artistPath = fs::path(folderPath).parent_path().string();
    bool purgeArtist = false;
    int artistDeleted = 0;
    if (!artistPath.empty() && fs::exists(artistPath)) {
      auto remainingAlbums = db->getAlbumsRaw(targetArtist);
      bool hasAlbums = false;
      for (const auto &[album, artist, year] : remainingAlbums) {
        if (artist == targetArtist) {
          hasAlbums = true;
          break;
        }
      }
      if (!hasAlbums) {
        bool hasFiles = false;
        try {
          for (const auto &entry :
               fs::recursive_directory_iterator(artistPath)) {
            if (fs::is_regular_file(entry.path())) {
              hasFiles = true;
              break;
            }
          }
        } catch (...) {
        }
        if (!hasFiles) {
          purgeArtist = true;
        }
      }
    }
    if (purgeArtist && fs::exists(artistPath)) {
      if (fsService.moveToTrash(artistPath)) {
        artistDeleted = 1;
      }
    }
    musicRepository.waitForPendingReload();
    nlohmann::json responseData;
    responseData["success"] = true;
    responseData["deletedFiles"] = countDeleted;
    responseData["errorCount"] = 0;
    responseData["album"] = targetAlbum;
    responseData["artist"] = targetArtist;
    responseData["albumFolder"] = folderPath;
    responseData["artistFolderDeleted"] = (artistDeleted == 1);
    if (purgeArtist) {
      responseData["artistFolder"] = artistPath;
    }
    response.setStatus(200);
    response.setJsonContent(responseData.dump());
  } catch (const std::exception &exceptionPayload) {
    response.setStatus(500);
    response.setJsonContent(
        this->error_response(500, exceptionPayload.what()).dump());
  }
  return response;
}

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

nlohmann::json
AlbumManagementController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}

void AlbumManagementController::logAlbumState(const std::string &tag,
                                              const std::string &album,
                                              const std::string &artist) {
  std::cerr << "[DEL_ALBUM][" << tag << "] album='" << album << "' artist='"
            << artist << "'" << std::endl;
  auto tracksInDb = db->getTracksByAlbumRaw(album, artist);
  std::cerr << "[DEL_ALBUM][" << tag << "] tracksInDb=" << tracksInDb.size()
            << std::endl;
  for (const auto &t : tracksInDb) {
    bool exists = fs::exists(t.filePath);
    std::cerr << "[DEL_ALBUM][" << tag << "]   db: '" << t.filePath
              << "' fsExists=" << exists << std::endl;
  }
  auto albumsInDb = db->getAlbumsRaw("");
  std::cerr << "[DEL_ALBUM][" << tag
            << "] totalAlbumsInDb=" << albumsInDb.size() << std::endl;
  bool albumPresent = false;
  for (const auto &[a, ar, y] : albumsInDb) {
    if (a == album && ar == artist) {
      albumPresent = true;
      std::cerr << "[DEL_ALBUM][" << tag << "]   albumPresent: '" << a
                << "' / '" << ar << "'" << std::endl;
    }
  }
  std::cerr << "[DEL_ALBUM][" << tag
            << "] albumPresentInAlbumsRaw=" << albumPresent << std::endl;
}
