#pragma once

#include "services/PlayerService.h"
#include <drogon/HttpController.h>
#include <json/json.h>
#include <memory>

class PlayerController : public drogon::HttpController<PlayerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PlayerController::handlePlay, "/api/play", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePause, "/api/pause", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleStop, "/api/stop", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNext, "/api/next", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePrevious, "/api/previous",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSetTrack, "/api/track", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSetPlaylist, "/api/setPlaylist",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleAddToPlaylist, "/api/add",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleAddAfterCurrent, "/api/addAfterCurrent",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePlayIndex, "/api/playIndex",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleClear, "/api/clear", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleGetPlaylist, "/api/getPlaylist",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handleGetPlaybackState, "/api/playbackState",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handleGetCurrentTrack, "/api/currentTrack",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handleGetCurrentTime, "/api/currentTime",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handleRemoveFromPlaylist,
                "/api/removeFromPlaylist", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSeek, "/api/seek", drogon::Post);
  METHOD_LIST_END

  PlayerController();
  static void setPlayerService(std::shared_ptr<PlayerService> service);

  void
  handlePlay(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handlePause(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleStop(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleNext(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handlePrevious(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSetTrack(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleAddToPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleAddAfterCurrent(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handlePlayIndex(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleClear(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetPlaybackState(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetCurrentTrack(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetCurrentTime(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleRemoveFromPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleSeek(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  static std::shared_ptr<PlayerService> playerService_;
  Json::Value parseBody(const drogon::HttpRequestPtr &req);
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
};
