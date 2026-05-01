#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include <string>
#include <vector>

class PlayerController : public drogon::HttpController<PlayerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PlayerController::handlePlay, "/api/audio/play", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePause, "/api/audio/pause",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleStop, "/api/audio/stop", drogon::Post);
  ADD_METHOD_TO(PlayerController::handleNext, "/api/audio/next", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePrevious, "/api/audio/previous",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleSetPlaylist, "/api/audio/setPlaylist",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleAddToPlaylist, "/api/audio/add",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleClear, "/api/audio/clear",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleGetPlaylist, "/api/audio/getPlaylist",
                drogon::Get);
  ADD_METHOD_TO(PlayerController::handlePlaybackState,
                "/api/audio/playbackState", drogon::Get);
  ADD_METHOD_TO(PlayerController::handleGetCurrentTime,
                "/api/audio/currentTime", drogon::Get);
  ADD_METHOD_TO(PlayerController::handleSeek, "/api/audio/seek", drogon::Post);
  ADD_METHOD_TO(PlayerController::handlePlayFile, "/api/audio/playFile",
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
  ADD_METHOD_TO(PlayerController::handlePlayIndex, "/api/audio/playIndex",
                drogon::Post);
  ADD_METHOD_TO(PlayerController::handleForceStop, "/api/audio/forceStop",
                drogon::Post);
  METHOD_LIST_END

  PlayerController();
  ~PlayerController();

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
  void handleSetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleAddToPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleClear(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetPlaylist(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handlePlaybackState(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleGetCurrentTime(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleSeek(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handlePlayFile(
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
  void handlePlayIndex(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleForceStop(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  Json::Value parseBody(const drogon::HttpRequestPtr &req);
  Json::Value jsonResponse(bool success, const std::string &message = "",
                           const Json::Value &data = Json::Value());
  std::string sendCommand(const std::string &jsonCmd);
  void launchMpv();
  bool isProcessAlive();
  void stopMpv();
  void loadTrack(int index);
  std::string escapePath(const std::string &path);
  void startAutoAdvance();
  std::string socketPath_;
  std::vector<std::string> playlist_;
  int currentIndex_;
  static int instanceCounter_;
};
