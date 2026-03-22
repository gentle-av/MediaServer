#pragma once

#include "database/MusicDatabase.h"
#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace drogon;

class MusicController : public drogon::HttpController<MusicController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicController::getAlbums, "/api/music/albums", Post, Options);
  ADD_METHOD_TO(MusicController::listFiles, "/api/music/list", Post, Options);
  ADD_METHOD_TO(MusicController::openAudio, "/api/music/open", Post, Options);
  ADD_METHOD_TO(MusicController::getAlbumArt, "/api/music/albumart", Get,
                Options);
  ADD_METHOD_TO(MusicController::getArtists, "/api/music/artists", Post,
                Options);
  ADD_METHOD_TO(MusicController::rescanLibrary, "/api/music/rescan", Post,
                Options);
  METHOD_LIST_END

  MusicController();
  void getAlbums(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void listFiles(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void openAudio(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void getAlbumArt(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
  void getArtists(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
  void rescanLibrary(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

private:
  std::string getMimeType(const std::string &extension);
  Json::Value getFileInfo(const fs::path &path);
  bool isAudioFile(const std::string &filename);
  std::string formatFileSize(uintmax_t size);
  void addCorsHeaders(const HttpResponsePtr &resp);
  void playFile(const std::string &path);
  void stopCurrentPlayback();
  void launchPlayerWithFile(const std::string &path);
  std::string getPlayerCommand(const std::string &path);

  struct MusicState {
    std::string currentFile;
    std::vector<std::string> playlist;
    int currentIndex = -1;
    bool isPlaying = false;
    bool isPaused = false;
    int volume = 70;
    std::string playerPid;
  };

  MusicState state;
  std::mutex stateMutex;
  std::unique_ptr<MusicDatabase> db_;
  std::mutex scanMutex_;
  bool isScanning_;
};
