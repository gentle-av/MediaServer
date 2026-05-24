#include "controllers/albums/AlbumManagementController.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>

namespace fs = std::filesystem;

MusicDatabase *AlbumManagementController::db_ = nullptr;
MetadataCache *AlbumManagementController::cache_ = nullptr;

AlbumManagementController::AlbumManagementController() {}

void AlbumManagementController::init(std::unique_ptr<MusicDatabase> &db,
                                     std::unique_ptr<MetadataCache> &cache) {
  db_ = db.get();
  cache_ = cache.get();
}

void AlbumManagementController::deleteAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json || !json->isMember("album") || !json->isMember("artist")) {
    ResponseBuilder::sendError(callback, "Missing album or artist parameter");
    return;
  }
  std::string albumName = (*json)["album"].asString();
  std::string artistName = (*json)["artist"].asString();
  std::string decodedAlbum = drogon::utils::urlDecode(albumName);
  std::string decodedArtist = drogon::utils::urlDecode(artistName);
  try {
    auto tracks = db_->getTracksByAlbum(decodedAlbum, decodedArtist);
    if (tracks.empty()) {
      ResponseBuilder::sendError(callback, "Album not found",
                                 drogon::k404NotFound);
      return;
    }
    std::string albumFolderPath;
    for (const auto &track : tracks) {
      fs::path trackPath(track.filePath);
      albumFolderPath = trackPath.parent_path().string();
      break;
    }
    if (albumFolderPath.empty()) {
      ResponseBuilder::sendError(callback,
                                 "Could not determine album folder path",
                                 drogon::k500InternalServerError);
      return;
    }
    for (const auto &track : tracks) {
      db_->removeFile(track.filePath);
      cache_->erase(track.filePath);
    }
    int deletedFiles = 0;
    int errorCount = 0;
    if (fs::exists(albumFolderPath)) {
      std::string trashCmd =
          "kioclient5 move \"" + albumFolderPath + "\" trash:/ 2>/dev/null";
      if (system(trashCmd.c_str()) != 0) {
        std::string rmCmd = "rm -rf \"" + albumFolderPath + "\"";
        if (system(rmCmd.c_str()) != 0) {
          errorCount++;
        } else {
          deletedFiles = static_cast<int>(tracks.size());
        }
      } else {
        deletedFiles = static_cast<int>(tracks.size());
      }
    } else {
      errorCount++;
    }
    std::string artistFolderPath =
        fs::path(albumFolderPath).parent_path().string();
    bool shouldDeleteArtistFolder = false;
    if (!artistFolderPath.empty() && fs::exists(artistFolderPath)) {
      auto remainingAlbums = db_->getAlbums(decodedArtist);
      bool hasOtherAlbums = false;
      for (const auto &[album, artist, year] : remainingAlbums) {
        if (artist == decodedArtist) {
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
    int artistFolderDeleted = 0;
    if (shouldDeleteArtistFolder && fs::exists(artistFolderPath)) {
      std::string artistTrashCmd =
          "kioclient5 move \"" + artistFolderPath + "\" trash:/ 2>/dev/null";
      if (system(artistTrashCmd.c_str()) != 0) {
        std::string artistRmCmd = "rm -rf \"" + artistFolderPath + "\"";
        if (system(artistRmCmd.c_str()) == 0) {
          artistFolderDeleted = 1;
        }
      } else {
        artistFolderDeleted = 1;
      }
    }
    Json::Value data;
    data["deleted_files"] = deletedFiles;
    data["error_count"] = errorCount;
    data["album"] = decodedAlbum;
    data["artist"] = decodedArtist;
    data["album_folder"] = albumFolderPath;
    data["artist_folder_deleted"] = (artistFolderDeleted == 1);
    if (shouldDeleteArtistFolder) {
      data["artist_folder"] = artistFolderPath;
    }
    ResponseBuilder::sendSuccess(callback, data);
  } catch (const std::exception &e) {
    ResponseBuilder::sendError(callback, e.what(),
                               drogon::k500InternalServerError);
  }
}
