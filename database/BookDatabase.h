#pragma once

#include "../models/Book.h"
#include "../models/Bookmark.h"
#include <memory>
#include <string>
#include <vector>

class BookDatabase {
public:
  explicit BookDatabase(const std::string &dbPath);
  ~BookDatabase();
  bool init();
  void close();

  bool addBook(const Book &book);
  bool removeBook(const std::string &filePath);
  bool bookExists(const std::string &filePath);
  std::vector<Book> getAllBooks();
  Book getBookByPath(const std::string &filePath);
  std::vector<char> getBookCover(const std::string &filePath);
  bool addBooksBatch(const std::vector<Book> &books);
  bool hasBookCover(const std::string &filePath);

  bool saveBookmark(const Bookmark &bookmark);
  std::vector<Bookmark> getBookmarks(const std::string &bookPath);
  bool removeBookmark(int bookmarkId);
  std::vector<std::string> getAllBookPaths();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};
