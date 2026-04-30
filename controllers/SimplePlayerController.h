#pragma once

#include <drogon/HttpController.h>
#include <json/json.h>
#include <string>
#include <vector>

class SimplePlayerController
    : public drogon::HttpController<SimplePlayerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(SimplePlayerController::handleNewPlay, "/api/new/play",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewPause, "/api/new/pause",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewStop, "/api/new/stop",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewNext, "/api/new/next",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewPrevious, "/api/new/previous",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewSetPlaylist,
                "/api/new/setPlaylist", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewAddToPlaylist, "/api/new/add",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewClear, "/api/new/clear",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewGetPlaylist,
                "/api/new/getPlaylist", drogon::Get);
  ADD_METHOD_TO(SimplePlayerController::handleNewGetPlaybackState,
                "/api/new/playbackState", drogon::Get);
  ADD_METHOD_TO(SimplePlayerController::handleNewGetCurrentTime,
                "/api/new/currentTime", drogon::Get);
  ADD_METHOD_TO(SimplePlayerController::handleNewSeek, "/api/new/seek",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleNewPlayFile, "/api/new/playFile",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleGetVolume, "/api/simple/volume",
                drogon::Get);
  ADD_METHOD_TO(SimplePlayerController::handleSetVolume, "/api/simple/volume",
                drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleIncreaseVolume,
                "/api/simple/volume/increase", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleDecreaseVolume,
                "/api/simple/volume/decrease", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleToggleMute,
                "/api/simple/volume/mute", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleSwitchToSpeakers,
                "/api/audio/output/speakers", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleSwitchToHeadphones,
                "/api/audio/output/headphones", drogon::Post);
  ADD_METHOD_TO(SimplePlayerController::handleGetAudioOutput,
                "/api/audio/output", drogon::Get);
  METHOD_LIST_END

  SimplePlayerController();
  ~SimplePlayerController();

  void handleNewPlay(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewPause(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewStop(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewNext(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewPrevious(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewSetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewAddToPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewClear(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewGetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewGetPlaybackState(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewGetCurrentTime(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewSeek(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleNewPlayFile(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetVolume(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSetVolume(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleIncreaseVolume(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleDecreaseVolume(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleToggleMute(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSwitchToSpeakers(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleSwitchToHeadphones(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetAudioOutput(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  Json::Value parseBody(const drogon::HttpRequestPtr &req);
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
  std::string sendCommand(const std::string &jsonCmd);
  void launchMpv();
  void killMpv();
  bool isProcessAlive();
  void updatePlaylistFromMpv();
  void startAutoAdvance();

  std::string socketPath_;
  std::vector<std::string> playlist_;
  int currentIndex_;
  static int instanceCounter_;
};
