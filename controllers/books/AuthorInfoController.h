#pragma once

#include "../../services/books/BookService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>

struct AuthorMeta {
  std::string full_name;
  int birth_year = 0;
  int death_year = 0;
  std::string photo_url;
  std::string photo_base64;
  std::string photo_mime;
  std::string biography;
};

class AuthorInfoController : public RestController<App> {
public:
  AuthorInfoController(App &app, std::shared_ptr<BookService> service,
                       const std::string &metaFilePath);
  ~AuthorInfoController() override = default;

protected:
  void register_all_routes() override;

private:
  StringHttpResponse handleAuthorInfo(const StringHttpRequest &req);
  StringHttpResponse handleAuthorPhoto(const StringHttpRequest &req);
  nlohmann::json buildAuthorInfo(const std::string &name);
  std::optional<AuthorMeta> findAuthorMeta(const std::string &name);
  void loadMetaCache();
  static std::vector<char> base64Decode(const std::string &base64Str);
  static std::string detectMime(const std::vector<char> &data);

  std::shared_ptr<BookService> bookService;
  std::string metaFilePath;
  std::mutex metaMutex;
  nlohmann::json metaCache;
  bool metaCacheLoaded = false;
};
