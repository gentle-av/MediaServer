#include "BookmarkController.h"

BookmarkController::BookmarkController(App &app,
                                       std::shared_ptr<BookService> service)
    : RestController<App>(app), bookService(std::move(service)) {}

void BookmarkController::register_all_routes() {
  app_.get("/api/bookmarks", [this](const StringHttpRequest &req) {
    return handleGetBookmarks(req);
  });
  app_.post("/api/bookmarks", [this](const StringHttpRequest &req) {
    return handleSaveBookmark(req);
  });
  app_.del("/api/bookmarks", [this](const StringHttpRequest &req) {
    return handleDeleteBookmark(req);
  });
}

StringHttpResponse
BookmarkController::handleGetBookmarks(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string path = req.getQuery("bookPath");
  auto bms = bookService->getBookmarks(path);
  nlohmann::json arr = nlohmann::json::array();
  for (const auto &b : bms) {
    nlohmann::json j;
    j["id"] = b.id;
    j["bookPath"] = b.bookPath;
    j["position"] = b.position;
    j["text"] = b.progressText;
    j["created"] = b.createdAt;
    j["updated"] = b.updatedAt;
    arr.push_back(j);
  }
  nlohmann::json resp;
  resp["success"] = true;
  resp["bookmarks"] = arr;
  res.setJsonContent(resp.dump());
  res.setStatus(200);
  return res;
}

StringHttpResponse
BookmarkController::handleSaveBookmark(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = nlohmann::json::parse(req.getBodyString());
    Bookmark bm;
    bm.bookPath = json["bookPath"].get<std::string>();
    bm.position = json["position"].get<double>();
    bm.progressText = json.value("text", "");
    if (bookService->saveBookmark(bm)) {
      res.setJsonContent(this->success_response("Bookmark saved").dump());
      res.setStatus(201);
    } else {
      res.setJsonContent(this->error_response(500, "DB error").dump());
      res.setStatus(500);
    }
  } catch (...) {
    res.setJsonContent(this->error_response(400, "Invalid JSON").dump());
    res.setStatus(400);
  }
  return res;
}

StringHttpResponse
BookmarkController::handleDeleteBookmark(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    int id = std::stoi(req.getQuery("id"));
    if (bookService->removeBookmark(id)) {
      res.setJsonContent(this->success_response("Deleted").dump());
      res.setStatus(200);
    } else {
      res.setJsonContent(this->error_response(404, "Not found").dump());
      res.setStatus(404);
    }
  } catch (...) {
    res.setJsonContent(this->error_response(400, "Invalid ID").dump());
    res.setStatus(400);
  }
  return res;
}
