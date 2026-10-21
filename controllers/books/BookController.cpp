#include "BookController.h"
#include <iostream>
#include <map>

BookController::BookController(App &app, std::shared_ptr<BookService> service)
    : RestController<App>(app), bookService(std::move(service)) {}

void BookController::register_all_routes() {
  app_.get("/api/books", [this](const StringHttpRequest &req) {
    return handleGetBooks(req);
  });
  app_.post("/api/books", [this](const StringHttpRequest &req) {
    return handleAddBook(req);
  });
  app_.del("/api/books", [this](const StringHttpRequest &req) {
    return handleDeleteBook(req);
  });
  app_.get("/api/books/cover", [this](const StringHttpRequest &req) {
    return handleGetCover(req);
  });
  app_.get("/api/books/authors", [this](const StringHttpRequest &req) {
    return handleGetAuthors(req);
  });
  app_.get("/api/books/by-author", [this](const StringHttpRequest &req) {
    return handleGetBooksByAuthor(req);
  });
}

StringHttpResponse
BookController::handleGetBooks(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto books = bookService->getAllBooks();
    std::cout << "[BookController::handleGetBooks] books size is "
              << books.size() << std::endl;
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &b : books) {
      nlohmann::json j;
      j["id"] = b.id;
      j["path"] = b.filePath;
      j["title"] = b.title;
      j["author"] = b.author;
      j["genre"] = b.genre;
      j["lang"] = b.language;
      j["sequence"] = b.sequence;
      j["seq_number"] = b.sequenceNumber;
      j["has_cover"] = bookService->hasBookCover(b.filePath);
      arr.push_back(j);
    }
    nlohmann::json resp;
    resp["success"] = true;
    resp["books"] = arr;
    std::string body =
        resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    std::cout << "[BookController::handleGetBooks] JSON dump OK, size="
              << body.size() << std::endl;
    res.setJsonContent(body);
    res.setStatus(200);
  } catch (const std::exception &e) {
    std::cerr << "[BookController::handleGetBooks] EXCEPTION: " << e.what()
              << std::endl;
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse BookController::handleAddBook(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = nlohmann::json::parse(req.getBodyString());
    std::string path = json["path"].get<std::string>();
    if (bookService->addBookFromFile(path)) {
      res.setJsonContent(this->success_response("Book added").dump());
      res.setStatus(201);
    } else {
      res.setJsonContent(
          this->error_response(500, "Failed to parse FB2 or save").dump());
      res.setStatus(500);
    }
  } catch (...) {
    res.setJsonContent(this->error_response(400, "Invalid JSON").dump());
    res.setStatus(400);
  }
  return res;
}

StringHttpResponse
BookController::handleDeleteBook(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string path = req.getQuery("path");
  if (bookService->removeBook(path)) {
    res.setJsonContent(this->success_response("Book removed").dump());
    res.setStatus(200);
  } else {
    res.setJsonContent(this->error_response(404, "Not found").dump());
    res.setStatus(404);
  }
  return res;
}

StringHttpResponse
BookController::handleGetCover(const StringHttpRequest &req) {
  StringHttpResponse res;
  std::string path = req.getQuery("path");
  auto cover = bookService->getBookCover(path);
  if (cover.empty()) {
    res.setStatus(404);
  } else {
    std::string mime = "image/jpeg";
    if (cover.size() > 4 && cover[0] == '\x89' && cover[1] == 'P')
      mime = "image/png";
    res.setHeader("Content-Type", mime);
    res.setBodyContent(std::string(cover.data(), cover.size()));
    res.setStatus(200);
  }
  return res;
}

StringHttpResponse
BookController::handleGetAuthors(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto books = bookService->getAllBooks();
    std::map<std::string, int> authorCounts;
    for (const auto &b : books) {
      std::string author = b.author;
      if (author.empty()) {
        author = "Unknown";
      }
      authorCounts[author]++;
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &[author, count] : authorCounts) {
      nlohmann::json j;
      j["author"] = author;
      j["count"] = count;
      arr.push_back(j);
    }
    nlohmann::json resp;
    resp["success"] = true;
    resp["authors"] = arr;
    resp["count"] = static_cast<int>(arr.size());
    res.setJsonContent(
        resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
BookController::handleGetBooksByAuthor(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string author = req.getQuery("author");
    if (author.empty()) {
      res.setStatus(400);
      res.setJsonContent(
          this->error_response(400, "Missing query parameter: author").dump());
      return res;
    }
    std::string normalizedAuthor = (author == "Unknown") ? "" : author;
    auto books = bookService->getAllBooks();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &b : books) {
      std::string bookAuthor = b.author.empty() ? "Unknown" : b.author;
      if (bookAuthor != author && b.author != normalizedAuthor) {
        continue;
      }
      nlohmann::json j;
      j["id"] = b.id;
      j["path"] = b.filePath;
      j["title"] = b.title;
      j["author"] = bookAuthor;
      j["genre"] = b.genre;
      j["lang"] = b.language;
      j["sequence"] = b.sequence;
      j["seq_number"] = b.sequenceNumber;
      j["has_cover"] = bookService->hasBookCover(b.filePath);
      arr.push_back(j);
    }
    nlohmann::json resp;
    resp["success"] = true;
    resp["author"] = author;
    resp["books"] = arr;
    resp["count"] = static_cast<int>(arr.size());
    res.setJsonContent(
        resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}
