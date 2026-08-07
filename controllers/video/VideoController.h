#pragma once
#include <atomic>
#include <chrono>
#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <memory>
#include <mutex>
#include <string>

class Profiler;
using namespace drogon;

class VideoController : public drogon::HttpController<VideoController, false> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(VideoController::getIndex, "/", Get);
  ADD_METHOD_TO(VideoController::serveStatic, "/static/{filename}", Get);
  ADD_METHOD_TO(VideoController::setAudioTrack, "/api/video/audio/track", Post);
  ADD_METHOD_TO(VideoController::listFiles, "/api/list", Post);
  ADD_METHOD_TO(VideoController::openVideo, "/api/open", Post);
  ADD_METHOD_TO(VideoController::moveToTrash, "/api/trash", Post);
  ADD_METHOD_TO(VideoController::getPlaybackStatus, "/api/video/status", Get);
  ADD_METHOD_TO(VideoController::controlMpv, "/api/mpv/control", Post);
  ADD_METHOD_TO(VideoController::seekMpv, "/api/mpv/seek", Post);
  ADD_METHOD_TO(VideoController::fastSeek, "/api/mpv/seek-fast", Post);
  ADD_METHOD_TO(VideoController::getMpvProperty, "/api/mpv/property/{name}",
                Get);
  ADD_METHOD_TO(VideoController::closeVideo, "/api/video/close", Post);
  ADD_METHOD_TO(VideoController::forceStopVideo, "/api/video/forceStop", Post);
  ADD_METHOD_TO(VideoController::deleteDirectory, "/api/delete-directory",
                Post);
  METHOD_LIST_END

  VideoController() = default;
  void init(std::shared_ptr<Profiler> profiler);

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
  void
  getPlaybackStatus(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
  void controlMpv(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
  void seekMpv(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
  void fastSeek(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);
  void getMpvProperty(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &propertyName);
  void closeVideo(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
  void forceStopVideo(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
  void deleteDirectory(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
  void setAudioTrack(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<Profiler> profiler_;
  static std::string activeSocket_;
  struct CachedStatus {
    Json::Value data;
    std::chrono::steady_clock::time_point timestamp;
    bool isValid = false;
  };
  CachedStatus statusCache_;
  std::mutex statusMutex_;
  std::atomic<bool> statusRequestInProgress_{false};
};
