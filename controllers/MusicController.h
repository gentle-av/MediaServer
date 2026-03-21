#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace drogon;

class MusicController : public drogon::HttpController<MusicController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MusicController::listFiles, "/api/music/list", Post, Options);
  ADD_METHOD_TO(MusicController::openAudio, "/api/music/open", Post, Options);

  ADD_METHOD_TO(MusicController::play, "/api/music/play", Post, Options);
  ADD_METHOD_TO(MusicController::pause, "/api/music/pause", Post, Options);
  ADD_METHOD_TO(MusicController::next, "/api/music/next", Post, Options);
  ADD_METHOD_TO(MusicController::previous, "/api/music/previous", Post,
                Options);
  ADD_METHOD_TO(MusicController::stop, "/api/music/stop", Post, Options);
  ADD_METHOD_TO(MusicController::clear, "/api/music/clear", Post, Options);

  ADD_METHOD_TO(MusicController::getStatus, "/api/music/status", Post, Options);
  ADD_METHOD_TO(MusicController::getPlaylist, "/api/music/playlist", Post,
                Options);
  ADD_METHOD_TO(MusicController::addToPlaylist, "/api/music/add", Post,
                Options);
  ADD_METHOD_TO(MusicController::removeFromPlaylist, "/api/music/remove", Post,
                Options);
  METHOD_LIST_END

  void listFiles(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void openAudio(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  void play(const HttpRequestPtr &req,
            std::function<void(const HttpResponsePtr &)> &&callback);
  void pause(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback);
  void next(const HttpRequestPtr &req,
            std::function<void(const HttpResponsePtr &)> &&callback);
  void previous(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);
  void stop(const HttpRequestPtr &req,
            std::function<void(const HttpResponsePtr &)> &&callback);
  void clear(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback);

  void getStatus(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void getPlaylist(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
  void addToPlaylist(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
  void
  removeFromPlaylist(const HttpRequestPtr &req,
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
};
