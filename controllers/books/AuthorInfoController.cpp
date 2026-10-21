#include "AuthorInfoController.h"
#include <filesystem>
#include <fstream>

AuthorInfoController::AuthorInfoController(
    App &app, std::shared_ptr<BookService> service,
    const std::string &metaFilePath)
    : RestController<App>(app), bookService(std::move(service)),
      metaFilePath(metaFilePath) {}

void AuthorInfoController::register_all_routes() {
  this->app_.get("/api/books/author-info",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleAuthorInfo(req);
                 });
  this->app_.get("/api/books/author-photo",
                 [this](const StringHttpRequest &req) -> StringHttpResponse {
                   return this->handleAuthorPhoto(req);
                 });
}

StringHttpResponse
AuthorInfoController::handleAuthorInfo(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getQuery("name");
    if (name.empty()) {
      res.setStatus(400);
      res.setJsonContent(this->error_response(400, "Missing name").dump());
      return res;
    }
    nlohmann::json data = buildAuthorInfo(name);
    nlohmann::json resp;
    resp["success"] = true;
    resp["data"] = std::move(data);
    res.setJsonContent(resp.dump(-1, ' ', false,
                                 nlohmann::json::error_handler_t::replace));
    res.setStatus(200);
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

StringHttpResponse
AuthorInfoController::handleAuthorPhoto(const StringHttpRequest &req) {
  StringHttpResponse res;
  try {
    std::string name = req.getQuery("name");
    if (name.empty()) {
      res.setStatus(400);
      return res;
    }
    auto meta = findAuthorMeta(name);
    if (meta && !meta->photo_base64.empty()) {
      auto data = base64Decode(meta->photo_base64);
      if (!data.empty()) {
        std::string mime =
            meta->photo_mime.empty() ? detectMime(data) : meta->photo_mime;
        res.setHeader("Content-Type", mime);
        res.setHeader("Content-Length", std::to_string(data.size()));
        res.setBodyContent(std::string(data.data(), data.size()));
        res.setStatus(200);
        return res;
      }
    }
    auto books = bookService->getAllBooks();
    for (const auto &b : books) {
      std::string bookAuthor = b.author.empty() ? "Unknown" : b.author;
      if (bookAuthor != name) continue;
      auto cover = bookService->getBookCover(b.filePath);
      if (!cover.empty()) {
        std::string mime = detectMime(cover);
        res.setHeader("Content-Type", mime);
        res.setHeader("Content-Length", std::to_string(cover.size()));
        res.setBodyContent(std::string(cover.data(), cover.size()));
        res.setStatus(200);
        return res;
      }
    }
    res.setStatus(404);
    res.setJsonContent("{\"error\":\"Author photo not found\"}");
  } catch (const std::exception &e) {
    res.setStatus(500);
    res.setJsonContent(this->error_response(500, e.what()).dump());
  }
  return res;
}

nlohmann::json AuthorInfoController::buildAuthorInfo(const std::string &name) {
  nlohmann::json data;
  data["name"] = name;
  data["full_name"] = name;
  data["birth_year"] = nullptr;
  data["death_year"] = nullptr;
  data["photo_url"] = nullptr;
  data["photo_base64"] = nullptr;
  data["photo_mime"] = nullptr;
  data["biography"] = nullptr;
  auto meta = findAuthorMeta(name);
  if (meta) {
    if (!meta->full_name.empty()) data["full_name"] = meta->full_name;
    if (meta->birth_year > 0) data["birth_year"] = meta->birth_year;
    if (meta->death_year > 0) data["death_year"] = meta->death_year;
    if (!meta->photo_url.empty()) data["photo_url"] = meta->photo_url;
    if (!meta->photo_base64.empty()) data["photo_base64"] = meta->photo_base64;
    if (!meta->photo_mime.empty()) data["photo_mime"] = meta->photo_mime;
    if (!meta->biography.empty()) data["biography"] = meta->biography;
  }
  if (!data.contains("photo_url") || data["photo_url"].is_null()) {
    data["photo_url"] =
        "/api/books/author-photo?name=" +
        std::string(StringHttpRequest::urlEncode(name));
  }
  nlohmann::json arr = nlohmann::json::array();
  auto books = bookService->getAllBooks();
  for (const auto &b : books) {
    std::string bookAuthor = b.author.empty() ? "Unknown" : b.author;
    if (bookAuthor != name) continue;
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
    arr.push_back(std::move(j));
  }
  data["books"] = std::move(arr);
  return data;
}

std::optional<AuthorMeta>
AuthorInfoController::findAuthorMeta(const std::string &name) {
  std::lock_guard<std::mutex> lock(metaMutex);
  if (!metaCacheLoaded) {
    loadMetaCache();
    metaCacheLoaded = true;
  }
  if (!metaCache.contains("authors") || !metaCache["authors"].is_array()) {
    return std::nullopt;
  }
  for (const auto &entry : metaCache["authors"]) {
    if (!entry.is_object()) continue;
    std::string entryName;
    if (entry.contains("name") && entry["name"].is_string()) {
      entryName = entry["name"].get<std::string>();
    } else if (entry.contains("full_name") && entry["full_name"].is_string()) {
      entryName = entry["full_name"].get<std::string>();
    }
    if (entryName != name) continue;
    AuthorMeta meta;
    if (entry.contains("full_name") && entry["full_name"].is_string())
      meta.full_name = entry["full_name"].get<std::string>();
    if (entry.contains("birth_year") && entry["birth_year"].is_number_integer())
      meta.birth_year = entry["birth_year"].get<int>();
    if (entry.contains("death_year") && entry["death_year"].is_number_integer())
      meta.death_year = entry["death_year"].get<int>();
    if (entry.contains("photo_url") && entry["photo_url"].is_string())
      meta.photo_url = entry["photo_url"].get<std::string>();
    if (entry.contains("photo_base64") && entry["photo_base64"].is_string())
      meta.photo_base64 = entry["photo_base64"].get<std::string>();
    if (entry.contains("photo_mime") && entry["photo_mime"].is_string())
      meta.photo_mime = entry["photo_mime"].get<std::string>();
    if (entry.contains("biography") && entry["biography"].is_string())
      meta.biography = entry["biography"].get<std::string>();
    return meta;
  }
  return std::nullopt;
}

void AuthorInfoController::loadMetaCache() {
  metaCache = nlohmann::json::object();
  if (metaFilePath.empty() || !std::filesystem::exists(metaFilePath)) {
    return;
  }
  std::ifstream file(metaFilePath);
  if (!file.is_open()) {
    return;
  }
  try {
    metaCache = nlohmann::json::parse(file);
  } catch (const std::exception &e) {
    std::cerr << "[AuthorInfoController] meta parse error: " << e.what()
              << std::endl;
    metaCache = nlohmann::json::object();
  }
}

std::vector<char>
AuthorInfoController::base64Decode(const std::string &base64Str) {
  static const std::string b64Chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> b64Table(256, -1);
  for (int i = 0; i < 64; i++) b64Table[b64Chars[i]] = i;
  std::vector<char> decodedData;
  int bitBuffer = 0;
  int bitCount = -8;
  for (unsigned char currentChar : base64Str) {
    if (b64Table[currentChar] == -1) continue;
    bitBuffer = (bitBuffer << 6) + b64Table[currentChar];
    bitCount += 6;
    if (bitCount >= 0) {
      decodedData.push_back(char((bitBuffer >> bitCount) & 0xFF));
      bitCount -= 8;
    }
  }
  return decodedData;
}

std::string AuthorInfoController::detectMime(const std::vector<char> &data) {
  if (data.size() >= 4) {
    if (data[0] == (char)0xFF && data[1] == (char)0xD8) return "image/jpeg";
    if (data[0] == (char)0x89 && data[1] == (char)0x50) return "image/png";
    if (data[0] == (char)0x47 && data[1] == (char)0x49) return "image/gif";
  }
  return "application/octet-stream";
}
