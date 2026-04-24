#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <memory>
#include <string>

class Profiler;
class PlayerService;

namespace fs = std::filesystem;
using namespace drogon;

class VideoController : public drogon::HttpController<VideoController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(VideoController::getIndex, "/", Get);
  ADD_METHOD_TO(VideoController::serveStatic, "/static/{filename}", Get);
  ADD_METHOD_TO(VideoController::listFiles, "/api/list", Post);
  ADD_METHOD_TO(VideoController::openVideo, "/api/open", Post);
  ADD_METHOD_TO(VideoController::moveToTrash, "/api/trash", Post);
  ADD_METHOD_TO(VideoController::getThumbnail, "/api/thumbnail", Get);
  ADD_METHOD_TO(VideoController::getPlaybackStatus, "/api/video/status", Get);
  ADD_METHOD_TO(VideoController::controlMpv, "/api/mpv/control", Post);
  ADD_METHOD_TO(VideoController::seekMpv, "/api/mpv/seek", Post);
  ADD_METHOD_TO(VideoController::getMpvProperty, "/api/mpv/property/{name}",
                Get);
  METHOD_LIST_END

  void setProfiler(Profiler *profiler) { profiler_ = profiler; }
  static void setPlayerService(std::shared_ptr<PlayerService> service);

  void getIndex(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);
  void serveStatic(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &filename);
  void listFiles(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void openVideo(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  void moveToTrash(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
  void getThumbnail(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
  void
  getPlaybackStatus(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
  void controlMpv(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
  void seekMpv(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
  void getMpvProperty(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &propertyName);

private:
  Profiler *profiler_ = nullptr;
  static std::shared_ptr<PlayerService> playerService_;
  std::string getMimeType(const std::string &extension);
  Json::Value getFileInfo(const fs::path &path);
  bool isVideoFile(const std::string &filename);
  std::string formatFileSize(uintmax_t size);
  std::string activeSocket_;
};
