#include "BookService.h"
#include "Fb2MetadataExtractor.h"
#include <filesystem>
#include <iostream>

BookService::BookService(std::shared_ptr<BookDatabase> db)
    : db(std::move(db)) {}

bool BookService::addBookFromFile(const std::string &filePath) {
  if (!std::filesystem::exists(filePath))
    return false;
  if (db->bookExists(filePath))
    return true;
  Book book;
  if (Fb2MetadataExtractor::extractFull(filePath, book)) {
    return db->addBook(book);
  }
  return false;
}

bool BookService::removeBook(const std::string &filePath) {
  return db->removeBook(filePath);
}

std::vector<Book> BookService::getAllBooks() {
  std::cerr << "[BookService] getAllBooks called" << '\n';
  if (!db) {
    std::cerr << "[BookService] db is null!" << '\n';
    return {};
  }
  auto books = db->getAllBooks();
  std::cerr << "[BookService] books count: " << books.size() << '\n';
  return books;
}

std::vector<char> BookService::getBookCover(const std::string &filePath) {
  return db->getBookCover(filePath);
}

bool BookService::saveBookmark(const Bookmark &bookmark) {
  return db->saveBookmark(bookmark);
}

std::vector<Bookmark> BookService::getBookmarks(const std::string &bookPath) {
  return db->getBookmarks(bookPath);
}

bool BookService::removeBookmark(int id) { return db->removeBookmark(id); }

bool BookService::hasBookCover(const std::string &filePath) {
  return db->hasBookCover(filePath);
}
