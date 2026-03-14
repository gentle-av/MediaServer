#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace drogon;

class VideoController : public drogon::HttpController<VideoController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(VideoController::getIndex, "/", Get);
  ADD_METHOD_TO(VideoController::serveStatic, "/static/{filename}", Get);
  ADD_METHOD_TO(VideoController::listFiles, "/api/list", Post);
  ADD_METHOD_TO(VideoController::openVideo, "/api/open", Post);
  METHOD_LIST_END

  void getIndex(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

  void serveStatic(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &filename);

  void listFiles(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  void openVideo(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

private:
  std::string getMimeType(const std::string &extension);
  Json::Value getFileInfo(const fs::path &path);
  bool isVideoFile(const std::string &filename);
  std::string formatFileSize(uintmax_t size);
};
