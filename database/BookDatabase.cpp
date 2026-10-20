#include "BookDatabase.h"
#include <mutex>
#include <sqlite3.h>

class BookDatabase::Impl {
public:
  std::mutex databaseMutex;
  explicit Impl(const std::string &dbPath) : dbPath_(dbPath), db(nullptr) {}
  ~Impl() {
    if (db)
      sqlite3_close(db);
  }

  bool init() {
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
      return false;
    sqlite3_exec(db, "PRAGMA encoding = \"UTF-8\";", nullptr, nullptr, nullptr);
    const char *schema = R"(
            CREATE TABLE IF NOT EXISTS books (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_path TEXT UNIQUE NOT NULL,
                title TEXT, author TEXT, genre TEXT, lang TEXT,
                sequence TEXT, seq_number INTEGER,
                cover_data BLOB, cover_mime TEXT,
                added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            CREATE TABLE IF NOT EXISTS bookmarks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                book_path TEXT NOT NULL,
                position REAL NOT NULL,
                progress_text TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY(book_path) REFERENCES books(file_path) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_bookmarks_path ON bookmarks(book_path);
        )";
    char *err = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
      sqlite3_free(err);
      return false;
    }
    return true;
  }
  sqlite3 *getDb() { return db; }

private:
  std::string dbPath_;
  sqlite3 *db;
};

BookDatabase::BookDatabase(const std::string &dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {}
BookDatabase::~BookDatabase() = default;
bool BookDatabase::init() { return pImpl->init(); }
void BookDatabase::close() { pImpl.reset(); }

bool BookDatabase::addBook(const Book &book) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sql = "INSERT OR REPLACE INTO books (file_path, title, author, "
                    "genre, lang, sequence, seq_number, cover_data, "
                    "cover_mime) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, book.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, book.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, book.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, book.genre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, book.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, book.sequence.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, book.sequenceNumber);
    if (!book.coverData.empty()) {
      sqlite3_bind_blob(stmt, 8, book.coverData.data(), book.coverData.size(),
                        SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, book.coverMime.c_str(), -1, SQLITE_TRANSIENT);
    } else {
      sqlite3_bind_null(stmt, 8);
      sqlite3_bind_null(stmt, 9);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
  }
  return false;
}

bool BookDatabase::removeBook(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sql = "DELETE FROM books WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
  }
  return false;
}

bool BookDatabase::bookExists(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sql = "SELECT 1 FROM books WHERE file_path = ?";
  sqlite3_stmt *stmt;
  bool found = false;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }
  return found;
}

std::vector<Book> BookDatabase::getAllBooks() {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<Book> books;
  const char *sql = "SELECT id, file_path, title, author, genre, lang, "
                    "sequence, seq_number FROM books ORDER BY author, title";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      Book b;
      b.id = sqlite3_column_int(stmt, 0);
      auto getStr = [&](int col) {
        const char *t = (const char *)sqlite3_column_text(stmt, col);
        return t ? t : "";
      };
      b.filePath = getStr(1);
      b.title = getStr(2);
      b.author = getStr(3);
      b.genre = getStr(4);
      b.language = getStr(5);
      b.sequence = getStr(6);
      b.sequenceNumber = sqlite3_column_int(stmt, 7);
      books.push_back(std::move(b));
    }
    sqlite3_finalize(stmt);
  }
  return books;
}

Book BookDatabase::getBookByPath(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  Book b;
  const char *sql = "SELECT id, title, author, genre, lang, sequence, "
                    "seq_number FROM books WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      b.id = sqlite3_column_int(stmt, 0);
      auto getStr = [&](int col) {
        const char *t = (const char *)sqlite3_column_text(stmt, col);
        return t ? t : "";
      };
      b.filePath = filePath;
      b.title = getStr(1);
      b.author = getStr(2);
      b.genre = getStr(3);
      b.language = getStr(4);
      b.sequence = getStr(5);
      b.sequenceNumber = sqlite3_column_int(stmt, 6);
    }
    sqlite3_finalize(stmt);
  }
  return b;
}

std::vector<char> BookDatabase::getBookCover(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<char> data;
  const char *sql = "SELECT cover_data FROM books WHERE file_path = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const void *blob = sqlite3_column_blob(stmt, 0);
      int size = sqlite3_column_bytes(stmt, 0);
      if (blob && size > 0)
        data.assign((const char *)blob, (const char *)blob + size);
    }
    sqlite3_finalize(stmt);
  }
  return data;
}

bool BookDatabase::saveBookmark(const Bookmark &bm) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sql =
      "INSERT INTO bookmarks (book_path, position, progress_text, updated_at) "
      "VALUES (?, ?, ?, CURRENT_TIMESTAMP) "
      "ON CONFLICT(id) DO UPDATE SET position=excluded.position, "
      "progress_text=excluded.progress_text, updated_at=CURRENT_TIMESTAMP";
  const char *sqlInsert = "INSERT INTO bookmarks (book_path, position, "
                          "progress_text) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sqlInsert, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, bm.bookPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, bm.position);
    sqlite3_bind_text(stmt, 3, bm.progressText.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
  }
  return false;
}

std::vector<Bookmark> BookDatabase::getBookmarks(const std::string &bookPath) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  std::vector<Bookmark> bms;
  const char *sql =
      "SELECT id, book_path, position, progress_text, created_at, updated_at "
      "FROM bookmarks WHERE book_path = ? ORDER BY updated_at DESC";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, bookPath.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      Bookmark b;
      b.id = sqlite3_column_int(stmt, 0);
      auto getStr = [&](int col) {
        const char *t = (const char *)sqlite3_column_text(stmt, col);
        return t ? t : "";
      };
      b.bookPath = getStr(1);
      b.position = sqlite3_column_double(stmt, 2);
      b.progressText = getStr(3);
      b.createdAt = getStr(4);
      b.updatedAt = getStr(5);
      bms.push_back(std::move(b));
    }
    sqlite3_finalize(stmt);
  }
  return bms;
}

bool BookDatabase::removeBookmark(int bookmarkId) {
  std::lock_guard<std::mutex> lock(pImpl->databaseMutex);
  const char *sql = "DELETE FROM bookmarks WHERE id = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(pImpl->getDb(), sql, -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, bookmarkId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
  }
  return false;
}
