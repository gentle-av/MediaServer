#pragma once

#include "../../database/BookDatabase.h"
#include "../../models/Book.h"
#include "../../models/Bookmark.h"
#include <memory>
#include <vector>

class BookService {
public:
  explicit BookService(std::shared_ptr<BookDatabase> db);

  bool addBookFromFile(const std::string &filePath);
  bool removeBook(const std::string &filePath);
  std::vector<Book> getAllBooks();
  std::vector<char> getBookCover(const std::string &filePath);

  bool saveBookmark(const Bookmark &bookmark);
  std::vector<Bookmark> getBookmarks(const std::string &bookPath);
  bool removeBookmark(int id);
  bool hasBookCover(const std::string &filePath);

private:
  std::shared_ptr<BookDatabase> db;
};
