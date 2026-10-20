#pragma once
#include "../../repositories/BookRepository.h"
#include <atomic>
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>

class BookScanController : public RestController<App> {
public:
  explicit BookScanController(App &app, BookRepository &repo,
                              const std::string &booksDir);
  ~BookScanController() = default;

protected:
  void register_all_routes() override;

private:
  BookRepository &bookRepository;
  std::string booksDirectory;
  std::atomic<int> scanTotal{0};
  std::atomic<int> scanProcessed{0};
  std::atomic<bool> scanFinished{false};
  std::atomic<bool> scanSuccess{true};

  nlohmann::json handleStartScan(const StringHttpRequest &req);
  nlohmann::json handleScanStatus(const StringHttpRequest &req);
  nlohmann::json handleRemoveMissing(const StringHttpRequest &req);
  nlohmann::json handleStopScan(const StringHttpRequest &req);
};
