#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <json/json.h>
#include <string>

class ResponseBuilder {
public:
  static drogon::HttpResponsePtr
  success(const Json::Value &data = Json::Value());
  static drogon::HttpResponsePtr
  error(const std::string &message,
        drogon::HttpStatusCode code = drogon::k400BadRequest);
  static drogon::HttpResponsePtr
  jsonResponse(const Json::Value &json,
               drogon::HttpStatusCode code = drogon::k200OK);

  static void
  sendSuccess(std::function<void(const drogon::HttpResponsePtr &)> &callback,
              const Json::Value &data = Json::Value());
  static void
  sendError(std::function<void(const drogon::HttpResponsePtr &)> &callback,
            const std::string &message,
            drogon::HttpStatusCode code = drogon::k400BadRequest);

  static drogon::HttpResponsePtr
  compressedJson(const Json::Value &json, const drogon::HttpRequestPtr &req);
};
