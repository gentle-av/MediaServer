#pragma once

#include "player/Player.h"
#include <drogon/HttpController.h>
#include <json/json.h>

class PlayerController : public drogon::HttpController<PlayerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PlayerController::handlePlay, "/api/play", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePause, "/api/pause", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleStop, "/api/stop", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSetTrack, "/api/track", drogon::Post);
  METHOD_LIST_END

  PlayerController();
  static void setPlayer(std::shared_ptr<Player> player);

  void
  handlePlay(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handlePause(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleStop(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSetTrack(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  static std::shared_ptr<Player> player_;
  Json::Value parseBody(const drogon::HttpRequestPtr &req);
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
};
