#pragma once

#include "services/video/ThreadMonitor.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

using namespace drogon;

class ThreadMonitorController
    : public drogon::HttpController<ThreadMonitorController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ThreadMonitorController::getThreadReport,
                "/debug/threads/report", Get);
  ADD_METHOD_TO(ThreadMonitorController::getWaitingThreads,
                "/debug/threads/waiting", Get);
  ADD_METHOD_TO(ThreadMonitorController::getThreadCount, "/debug/threads/count",
                Get);
  ADD_METHOD_TO(ThreadMonitorController::checkThread,
                "/debug/threads/check/{id}", Get);
  METHOD_LIST_END

  void getThreadReport(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
  void
  getWaitingThreads(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
  void getThreadCount(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
  void checkThread(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &threadId);
};
