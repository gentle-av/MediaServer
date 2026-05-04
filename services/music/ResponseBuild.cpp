#include "services/music/ResponseBuilder.h"
#include <drogon/utils/Utilities.h>

drogon::HttpResponsePtr ResponseBuilder::success(const Json::Value &data) {
  Json::Value response;
  response["status"] = "success";
  if (!data.isNull())
    response["data"] = data;
  return drogon::HttpResponse::newHttpJsonResponse(response);
}

drogon::HttpResponsePtr ResponseBuilder::error(const std::string &message,
                                               drogon::HttpStatusCode code) {
  Json::Value response;
  response["status"] = "error";
  response["message"] = message;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(code);
  return resp;
}

drogon::HttpResponsePtr
ResponseBuilder::jsonResponse(const Json::Value &json,
                              drogon::HttpStatusCode code) {
  auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
  resp->setStatusCode(code);
  return resp;
}

void ResponseBuilder::sendSuccess(
    std::function<void(const drogon::HttpResponsePtr &)> &callback,
    const Json::Value &data) {
  callback(success(data));
}

void ResponseBuilder::sendError(
    std::function<void(const drogon::HttpResponsePtr &)> &callback,
    const std::string &message, drogon::HttpStatusCode code) {
  callback(error(message, code));
}

drogon::HttpResponsePtr
ResponseBuilder::compressedJson(const Json::Value &json,
                                const drogon::HttpRequestPtr &req) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::string jsonStr = Json::writeString(builder, json);
  bool clientSupportsGzip =
      req->getHeader("accept-encoding").find("gzip") != std::string::npos;
  if (clientSupportsGzip && jsonStr.size() > 1024) {
    std::string compressed =
        drogon::utils::gzipCompress(jsonStr.data(), jsonStr.size());
    if (!compressed.empty() && compressed.size() < jsonStr.size()) {
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(drogon::k200OK);
      resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
      resp->addHeader("Content-Encoding", "gzip");
      resp->setBody(std::move(compressed));
      return resp;
    }
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  resp->setBody(std::move(jsonStr));
  return resp;
}
