#include "MkvAudioChannelController.h"
#include "models/MkvAudioTrackInfo.h"
#include <format>
#include <memory>

MkvAudioChannelController::MkvAudioChannelController() = default;
MkvAudioChannelController::~MkvAudioChannelController() = default;

std::unique_ptr<MkvAudioTrackInfo>
MkvAudioChannelController::openReader(const std::string &filepath,
                                      Callback &callback) const {
  auto reader = std::make_unique<MkvAudioTrackInfo>();
  auto result = reader->openFile(filepath);
  if (!result.has_value()) {
    sendError(std::move(callback), k400BadRequest, result.error());
    return nullptr;
  }
  return reader;
}

Json::Value MkvAudioChannelController::trackToJson(
    const MkvAudioTrackInfo::AudioTrack &track) const {
  Json::Value json;
  json["stream_index"] = track.stream_index;
  json["codec_name"] = track.codec_name;
  json["language"] = track.language;
  json["title"] = track.title;
  json["sample_rate"] = track.sample_rate;
  json["channels"] = track.channels;
  json["channel_layout"] = track.channel_layout;
  json["bit_rate"] = static_cast<Json::Int64>(track.bit_rate);
  json["is_default"] = track.is_default;
  json["is_forced"] = track.is_forced;
  return json;
}

void MkvAudioChannelController::sendError(Callback &&callback,
                                          drogon::HttpStatusCode statusCode,
                                          const std::string &message) const {
  Json::Value response;
  response["error"] = true;
  response["message"] = message;
  auto resp = HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(statusCode);
  callback(resp);
}

void MkvAudioChannelController::sendSuccess(Callback &&callback,
                                            const Json::Value &data) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(k200OK);
  callback(resp);
}

void MkvAudioChannelController::getAudioTracks(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto filepath = req->getParameter("path");
  if (filepath.empty()) {
    sendError(std::move(callback), k400BadRequest, "Missing 'path' parameter");
    return;
  }
  auto reader = openReader(filepath, callback);
  if (!reader)
    return;
  Json::Value response;
  response["success"] = true;
  response["count"] = reader->getTrackCount();
  Json::Value tracks(Json::arrayValue);
  for (const auto &track : reader->getAudioTracks()) {
    tracks.append(trackToJson(track));
  }
  response["tracks"] = tracks;
  sendSuccess(std::move(callback), response);
}

void MkvAudioChannelController::getTrackByIndex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto filepath = req->getParameter("path");
  if (filepath.empty()) {
    sendError(std::move(callback), k400BadRequest, "Missing 'path' parameter");
    return;
  }
  auto indexParam = req->getParameter("index");
  if (indexParam.empty()) {
    sendError(std::move(callback), k400BadRequest, "Missing 'index' parameter");
    return;
  }
  int index = std::stoi(indexParam);
  auto reader = openReader(filepath, callback);
  if (!reader)
    return;
  if (index < 0 || index >= reader->getTrackCount()) {
    sendError(std::move(callback), k404NotFound,
              std::format("Track index {} out of range (0-{})", index,
                          reader->getTrackCount() - 1));
    return;
  }
  const auto &tracks = reader->getAudioTracks();
  Json::Value response;
  response["success"] = true;
  response["track"] = trackToJson(tracks[index]);
  sendSuccess(std::move(callback), response);
}

void MkvAudioChannelController::getTrackByStream(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto filepath = req->getParameter("path");
  if (filepath.empty()) {
    sendError(std::move(callback), k400BadRequest, "Missing 'path' parameter");
    return;
  }
  auto streamParam = req->getParameter("stream");
  if (streamParam.empty()) {
    sendError(std::move(callback), k400BadRequest,
              "Missing 'stream' parameter");
    return;
  }
  int stream_index = std::stoi(streamParam);
  auto reader = openReader(filepath, callback);
  if (!reader)
    return;
  auto trackOpt = reader->getTrackByStreamIndex(stream_index);
  if (!trackOpt.has_value()) {
    sendError(std::move(callback), k404NotFound,
              std::format("Stream index {} not found", stream_index));
    return;
  }
  Json::Value response;
  response["success"] = true;
  response["track"] = trackToJson(trackOpt.value());
  sendSuccess(std::move(callback), response);
}
