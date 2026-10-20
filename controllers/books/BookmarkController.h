#pragma once

#include "../../services/books/BookService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class BookmarkController : public RestController<App> {
public:
  BookmarkController(App &app, std::shared_ptr<BookService> service);

protected:
  void register_all_routes() override;

private:
  std::shared_ptr<BookService> bookService;
  StringHttpResponse handleGetBookmarks(const StringHttpRequest &req);
  StringHttpResponse handleSaveBookmark(const StringHttpRequest &req);
  StringHttpResponse handleDeleteBookmark(const StringHttpRequest &req);
};
