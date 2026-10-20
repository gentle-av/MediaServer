#pragma once

#include "../../services/books/BookService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

class BookController : public RestController<App> {
public:
  BookController(App &app, std::shared_ptr<BookService> service);

protected:
  void register_all_routes() override;

private:
  std::shared_ptr<BookService> bookService;
  StringHttpResponse handleGetBooks(const StringHttpRequest &req);
  StringHttpResponse handleAddBook(const StringHttpRequest &req);
  StringHttpResponse handleDeleteBook(const StringHttpRequest &req);
  StringHttpResponse handleGetCover(const StringHttpRequest &req);
};
