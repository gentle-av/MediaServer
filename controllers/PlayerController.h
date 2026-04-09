#pragma once

#include "Musium.h"
#include <drogon/HttpController.h>
#include <json/json.h>

class Controller : public drogon::HttpController<Controller> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Controller::handlePlay, "/api/play", drogon::Post);
  ADD_METHOD_TO(Controller::handlePause, "/api/pause", drogon::Post);
  ADD_METHOD_TO(Controller::handleStop, "/api/stop", drogon::Post);
  ADD_METHOD_TO(Controller::handleSetTrack, "/api/track", drogon::Post);
  METHOD_LIST_END

  Controller();
  static void setPlayer(std::shared_ptr<Musium> player);

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
  static std::shared_ptr<Musium> musium_;
  Json::Value parseBody(const drogon::HttpRequestPtr &req);
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
};
