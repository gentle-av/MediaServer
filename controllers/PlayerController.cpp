#include "Controller.h"
#include <filesystem>
#include <iostream>
#include <thread>

std::shared_ptr<Musium> Controller::musium_ = nullptr;

Controller::Controller() {}

void Controller::setPlayer(std::shared_ptr<Musium> player) { musium_ = player; }

Json::Value Controller::parseBody(const drogon::HttpRequestPtr &req) {
  auto body = req->getBody();
  Json::Value result;
  if (body.empty())
    return result;
  std::string bodyStr(body.data(), body.size());
  Json::CharReaderBuilder builder;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(bodyStr.c_str(), bodyStr.c_str() + bodyStr.size(), &result,
                &errors);
  return result;
}

Json::Value Controller::jsonResponse(bool success, const std::string &message,
                                     const Json::Value &data) {
  Json::Value resp;
  resp["success"] = success;
  if (!message.empty())
    resp["message"] = message;
  if (!data.empty())
    resp["data"] = data;
  return resp;
}

void Controller::handlePlay(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "[DEBUG] handlePlay called" << std::endl;
  std::thread([]() {
    if (musium_)
      musium_->play();
  }).detach();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback started"));
  callback(resp);
}

void Controller::handlePause(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "[DEBUG] handlePause called" << std::endl;
  std::thread([]() {
    if (musium_)
      musium_->pause();
  }).detach();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback paused"));
  callback(resp);
}

void Controller::handleStop(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "[DEBUG] handleStop called" << std::endl;
  std::thread([]() {
    if (musium_)
      musium_->stop();
  }).detach();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(
      jsonResponse(true, "Playback stopped"));
  callback(resp);
}

void Controller::handleSetTrack(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  std::cout << "[DEBUG] handleSetTrack called" << std::endl;
  try {
    Json::Value json = parseBody(req);
    if (!json.isMember("track") || !json["track"].isString()) {
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "Missing track parameter"));
      callback(resp);
      return;
    }
    std::string track = json["track"].asString();
    if (!std::filesystem::exists(track)) {
      std::cerr << "[ERROR] File not found: " << track << std::endl;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(
          jsonResponse(false, "File not found: " + track));
      callback(resp);
      return;
    }
    std::cout << "[DEBUG] Setting track: " << track << std::endl;
    std::thread([track]() {
      if (musium_) {
        musium_->setPlaylist({track});
        musium_->play();
      }
    }).detach();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(true, "Track set and playing"));
    callback(resp);
  } catch (const std::exception &e) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        jsonResponse(false, e.what()));
    callback(resp);
  }
}
