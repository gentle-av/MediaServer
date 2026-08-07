// controllers/albums/AlbumManagementController.cpp
#include "controllers/albums/AlbumManagementController.h"
#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <regex>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

void AlbumManagementController::init(std::shared_ptr<MusicDatabase> db,
                                     std::shared_ptr<MetadataCache> cache) {
  db_ = db;
  cache_ = cache;
}

static std::vector<std::string> splitIntoArgs(const std::string &cmd) {
  std::vector<std::string> args;
  std::regex re(R"((?:[^\s"]+|"[^"]*")+)");
  auto begin = std::sregex_iterator(cmd.begin(), cmd.end(), re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    std::string arg = it->str();
    if (arg.front() == '"' && arg.back() == '"') {
      arg = arg.substr(1, arg.length() - 2);
    }
    args.push_back(arg);
  }
  return args;
}

static bool executeCommand(const std::vector<std::string> &args) {
  if (args.empty())
    return false;
  std::vector<char *> argv;
  for (const auto &arg : args) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  argv.push_back(nullptr);
  pid_t pid = fork();
  if (pid == -1)
    return false;
  if (pid == 0) {
    execvp(argv[0], argv.data());
    _exit(127);
  }
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool moveToTrash(const std::string &path) {
  std::vector<std::string> cmd = {"kioclient5", "move", path, "trash:/"};
  return executeCommand(cmd);
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
      if (moveToTrash(albumFolderPath)) {
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
    if (shouldDeleteArtistFolder && fs::exists(artistFolderPath)) {
      if (moveToTrash(artistFolderPath)) {
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
