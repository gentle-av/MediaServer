#include "BookScanController.h"

BookScanController::BookScanController(App &app, BookRepository &repo,
                                       const std::string &booksDir)
    : RestController<App>(app), bookRepository(repo), booksDirectory(booksDir) {
}

void BookScanController::register_all_routes() {
  this->app_.post("/api/books/scan",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    StringHttpResponse res;
                    nlohmann::json result = handleStartScan(req);
                    res.setJsonContent(result.dump());
                    res.setStatus(result.value("success", false) ? 200 : 409);
                    return res;
                  });
  this->app_.get("/api/books/scan/status",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   StringHttpResponse res;
                   nlohmann::json result = handleScanStatus(req);
                   res.setJsonContent(result.dump());
                   res.setStatus(200);
                   return res;
                 });
  this->app_.post("/api/books/remove-missing",
                  [this](const StringHttpRequest &req) -> StringHttpResponse {
                    StringHttpResponse res;
                    nlohmann::json result = handleRemoveMissing(req);
                    res.setJsonContent(result.dump());
                    res.setStatus(200);
                    return res;
                  });
}

nlohmann::json
BookScanController::handleStartScan(const StringHttpRequest &req) {
  if (bookRepository.isScanning()) {
    auto resp = this->error_response(409, "Scan already in progress");
    resp["scanning"] = true;
    return resp;
  }
  std::string targetDir = booksDirectory;
  auto queryDir = req.getQuery("dir");
  if (!queryDir.empty()) {
    targetDir = queryDir;
  } else {
    auto body = req.getBodyString();
    if (!body.empty()) {
      try {
        auto json = nlohmann::json::parse(body);
        if (json.contains("dir") && json["dir"].is_string()) {
          targetDir = json["dir"].get<std::string>();
        }
      } catch (...) {
      }
    }
  }
  scanTotal = 0;
  scanProcessed = 0;
  scanFinished = false;
  scanSuccess = true;
  auto future = bookRepository.scanBooksDirectoryAsync(
      targetDir, [this](int total, int processed) {
        scanTotal = total;
        scanProcessed = processed;
      });
  std::thread([this, future]() mutable {
    bool ok = future.get();
    scanSuccess = ok;
    scanFinished = true;
  }).detach();
  nlohmann::json resp = this->success_response("Scan started");
  resp["scanning"] = true;
  resp["directory"] = targetDir;
  return resp;
}

nlohmann::json
BookScanController::handleScanStatus(const StringHttpRequest &req) {
  nlohmann::json resp;
  resp["success"] = true;
  resp["scanning"] = bookRepository.isScanning();
  resp["finished"] = scanFinished.load();
  resp["scan_result"] = scanSuccess.load() ? "ok" : "error";
  resp["total"] = scanTotal.load();
  resp["processed"] = scanProcessed.load();
  resp["progress"] = scanTotal.load() > 0
                         ? (scanProcessed.load() * 100 / scanTotal.load())
                         : 0;
  return resp;
}

nlohmann::json
BookScanController::handleRemoveMissing(const StringHttpRequest &req) {
  return handleStartScan(req);
}
