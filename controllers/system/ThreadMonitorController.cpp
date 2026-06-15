#include "controllers/system/ThreadMonitorController.h"
#include <nlohmann/json.hpp>
#include <sstream>

void ThreadMonitorController::getThreadReport(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &monitor = ThreadMonitor::getInstance();
  std::string report = monitor.getWaitReport();
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->setContentTypeCode(CT_TEXT_PLAIN);
  resp->setBody(report);
  callback(resp);
}

void ThreadMonitorController::getWaitingThreads(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &monitor = ThreadMonitor::getInstance();
  auto waiting = monitor.getWaitingThreads();
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  nlohmann::json responseJson;
  responseJson["waiting_threads"] = nlohmann::json::array();
  responseJson["count"] = waiting.size();
  for (const auto &pair : waiting) {
    std::stringstream ss;
    ss << pair.first;
    nlohmann::json threadInfo;
    threadInfo["id"] = ss.str();
    threadInfo["name"] = pair.second;
    responseJson["waiting_threads"].push_back(threadInfo);
  }
  resp->setBody(responseJson.dump());
  callback(resp);
}

void ThreadMonitorController::getThreadCount(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &monitor = ThreadMonitor::getInstance();
  auto waiting = monitor.getWaitingThreads();
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  nlohmann::json responseJson;
  responseJson["total_waiting"] = waiting.size();
  resp->setBody(responseJson.dump());
  callback(resp);
}

void ThreadMonitorController::checkThread(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &threadId) {
  auto &monitor = ThreadMonitor::getInstance();
  auto waiting = monitor.getWaitingThreads();
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  nlohmann::json responseJson;
  responseJson["thread_id"] = threadId;
  responseJson["is_waiting"] = false;
  for (const auto &pair : waiting) {
    std::stringstream ss;
    ss << pair.first;
    if (ss.str() == threadId) {
      responseJson["is_waiting"] = true;
      responseJson["thread_name"] = pair.second;
      break;
    }
  }
  resp->setBody(responseJson.dump());
  callback(resp);
}
