#pragma once

#include "models/MkvAudioTrackInfo.h"
#include <drogon/HttpController.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include <memory>
#include <string>

using namespace drogon;

class MkvAudioTrackInfo;

class MkvAudioChannelController
    : public HttpController<MkvAudioChannelController> {
public:
  MkvAudioChannelController();
  ~MkvAudioChannelController() override;

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MkvAudioChannelController::getAudioTracks,
                "/api/audio/tracks/{filepath}", Get);
  ADD_METHOD_TO(MkvAudioChannelController::getTrackByIndex,
                "/api/audio/tracks/{filepath}/{index}", Get);
  ADD_METHOD_TO(MkvAudioChannelController::getTrackByStream,
                "/api/audio/tracks/stream/{filepath}/{stream_index}", Get);
  METHOD_LIST_END

  void getAudioTracks(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &filepath);

  void getTrackByIndex(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       const std::string &filepath, int index);

  void getTrackByStream(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        const std::string &filepath, int stream_index);

private:
  using Callback = std::function<void(const HttpResponsePtr &)>;

  [[nodiscard]] std::unique_ptr<MkvAudioTrackInfo>
  openReader(const std::string &filepath, Callback &callback) const;

  [[nodiscard]] Json::Value
  trackToJson(const MkvAudioTrackInfo::AudioTrack &track) const;

  void sendError(Callback &&callback, drogon::HttpStatusCode,
                 const std::string &message) const;
  void sendSuccess(Callback &&callback, const Json::Value &data) const;
};
