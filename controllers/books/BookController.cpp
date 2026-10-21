#include "BookController.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

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
  app_.post("/api/books/list", [this](const StringHttpRequest &req) {
    return handleListBooks(req);
  });
  app_.post("/api/books/content", [this](const StringHttpRequest &req) {
    return handleGetBookContent(req);
  });
  app_.post("/api/books/meta", [this](const StringHttpRequest &req) {
    return handleGetBookMeta(req);
  });
  app_.post("/api/books/progress", [this](const StringHttpRequest &req) {
    return handleSaveProgress(req);
  });
  app_.get("/api/books/progress", [this](const StringHttpRequest &req) {
    return handleGetProgress(req);
  });
}

StringHttpResponse
BookController::handleListBooks(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string path = "/mnt/books";
    auto body = req.getBodyString();
    if (!body.empty()) {
      auto json = nlohmann::json::parse(body);
      if (json.contains("path") && json["path"].is_string()) {
        path = json["path"].get<std::string>();
      }
    }
    if (!std::filesystem::exists(path) ||
        !std::filesystem::is_directory(path)) {
      nlohmann::json resp;
      resp["success"] = false;
      resp["error"] = "Directory not found: " + path;
      res.setJsonContent(resp.dump());
      res.setStatus(404);
      return res;
    }
    nlohmann::json items = nlohmann::json::array();
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      std::string name = entry.path().filename().string();
      if (!name.empty() && name[0] == '.') {
        continue;
      }
      nlohmann::json item;
      item["name"] = name;
      item["path"] = entry.path().string();
      bool isDir = entry.is_directory();
      item["isDirectory"] = isDir;
      std::string ext =
          std::filesystem::path(name).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      bool isBook = !isDir && (ext == ".fb2" || ext == ".epub" ||
                               ext == ".pdf" || ext == ".txt");
      item["isBook"] = isBook;
      if (isBook) {
        item["format"] = ext.empty() ? "" : ext.substr(1);
      }
      if (!isDir) {
        try {
          auto size = std::filesystem::file_size(entry.path());
          std::stringstream ss;
          ss << size << " B";
          item["size"] = ss.str();
        } catch (...) {
        }
      }
      items.push_back(item);
    }
    nlohmann::json resp;
    resp["success"] = true;
    resp["path"] = path;
    resp["items"] = items;
    res.setJsonContent(resp.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json resp;
    resp["success"] = false;
    resp["error"] = e.what();
    res.setJsonContent(resp.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
BookController::handleGetBookContent(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = nlohmann::json::parse(req.getBodyString());
    std::string path = json.value("path", "");
    if (path.empty() || !std::filesystem::exists(path)) {
      nlohmann::json resp;
      resp["success"] = false;
      resp["error"] = "File not found";
      res.setJsonContent(resp.dump());
      res.setStatus(404);
      return res;
    }
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    nlohmann::json resp;
    resp["success"] = true;
    resp["content"] = buffer.str();
    res.setJsonContent(resp.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json resp;
    resp["success"] = false;
    resp["error"] = e.what();
    res.setJsonContent(resp.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
BookController::handleGetBookMeta(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = nlohmann::json::parse(req.getBodyString());
    std::string path = json.value("path", "");
    if (path.empty()) {
      nlohmann::json resp;
      resp["success"] = false;
      resp["error"] = "Missing path";
      res.setJsonContent(resp.dump());
      res.setStatus(400);
      return res;
    }
    nlohmann::json data;
    data["format"] =
        std::filesystem::path(path).extension().string();
    nlohmann::json resp;
    resp["success"] = true;
    resp["data"] = data;
    res.setJsonContent(resp.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json resp;
    resp["success"] = false;
    resp["error"] = e.what();
    res.setJsonContent(resp.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
BookController::handleSaveProgress(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto json = nlohmann::json::parse(req.getBodyString());
    std::string path = json.value("path", "");
    int position = json.value("position", 0);
    if (path.empty()) {
      nlohmann::json resp;
      resp["success"] = false;
      resp["error"] = "Missing path";
      res.setJsonContent(resp.dump());
      res.setStatus(400);
      return res;
    }
    Bookmark bm;
    bm.bookPath = path;
    bm.position = static_cast<double>(position);
    bm.progressText = std::to_string(position);
    bookService->saveBookmark(bm);
    nlohmann::json resp;
    resp["success"] = true;
    res.setJsonContent(resp.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json resp;
    resp["success"] = false;
    resp["error"] = e.what();
    res.setJsonContent(resp.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
BookController::handleGetProgress(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string path = req.getQuery("path");
    if (path.empty()) {
      auto body = req.getBodyString();
      if (!body.empty()) {
        auto json = nlohmann::json::parse(body);
        path = json.value("path", "");
      }
    }
    if (path.empty()) {
      nlohmann::json resp;
      resp["success"] = false;
      resp["error"] = "Missing path";
      res.setJsonContent(resp.dump());
      res.setStatus(400);
      return res;
    }
    auto bms = bookService->getBookmarks(path);
    int position = 0;
    if (!bms.empty()) {
      position = static_cast<int>(bms.front().position);
    }
    nlohmann::json resp;
    resp["success"] = true;
    resp["position"] = position;
    res.setJsonContent(resp.dump());
    res.setStatus(200);
  } catch (const std::exception &e) {
    nlohmann::json resp;
    resp["success"] = false;
    resp["error"] = e.what();
    res.setJsonContent(resp.dump());
    res.setStatus(500);
  }
  return res;
}

StringHttpResponse
BookController::handleGetBooks(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    auto books = bookService->getAllBooks();
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
    res.setJsonContent(resp.dump(-1, ' ', false,
                                nlohmann::json::error_handler_t::replace));
    res.setStatus(200);
  } catch (const std::exception &e) {
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
