#pragma once

#include <drogon/HttpController.h>
#include <json/json.h>
#include <string>
#include <vector>

class PlayerController : public drogon::HttpController<PlayerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PlayerController::handleNewPlay, "/api/audio/play",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewPause, "/api/audio/pause",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewStop, "/api/audio/stop",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewNext, "/api/audio/next",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewPrevious, "/api/audio/previous",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewSetPlaylist,
                "/api/audio/setPlaylist", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewAddToPlaylist, "/api/audio/add",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewClear, "/api/audio/clear",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewGetPlaylist,
                "/api/audio/getPlaylist", drogon::Get);
  ADD_METHOD_TO(PlayerController::handleNewGetPlaybackState,
                "/api/audio/playbackState", drogon::Get);
  ADD_METHOD_TO(PlayerController::handleNewGetCurrentTime,
                "/api/audio/currentTime", drogon::Get);
  ADD_METHOD_TO(PlayerController::handleNewSeek, "/api/audio/seek",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNewPlayFile, "/api/audio/playFile",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleGetVolume, "/api/audio/volume",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handleSetVolume, "/api/audio/volume",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleIncreaseVolume,
                "/api/audio/volume/increase", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleDecreaseVolume,
                "/api/audio/volume/decrease", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleToggleMute, "/api/audio/volume/mute",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSwitchToSpeakers,
                "/api/audio/output/speakers", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSwitchToHeadphones,
                "/api/audio/output/headphones", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleGetAudioOutput, "/api/audio/output",
                drogon::Get);
  METHOD_LIST_END

  PlayerController();
  ~PlayerController();

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
