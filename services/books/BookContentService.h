#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct BookSection {
  int index = 0;
  int depth = 0;
  std::string title;
  std::string html;
  size_t charOffset = 0;
  size_t charLength = 0;
};

struct BookContent {
  std::string title;
  std::string author;
  std::string language;
  std::string filePath;
  std::vector<BookSection> sections;
  size_t totalChars = 0;
  bool parsed = false;
};

class BookContentService {
public:
  BookContentService() = default;
  ~BookContentService() = default;
  std::optional<BookContent> getContent(const std::string &filePath);
  std::optional<BookSection> getSection(const std::string &filePath,
                                        int sectionIndex);
  std::optional<BookSection> getSectionByCharOffset(const std::string &filePath,
                                                    size_t offset);
  std::optional<BookSection> getSectionByProgress(const std::string &filePath,
                                                  double progress);
  std::vector<std::pair<int, std::string>>
  getTableOfContents(const std::string &filePath);
  void invalidateCache(const std::string &filePath);
  void invalidateAllCache();
  size_t getCacheSize() const;
  void setMaxCacheSize(size_t maxSize);
private:
  std::shared_ptr<BookContent> parseFb2(const std::string &filePath);
  std::string fb2SectionToHtml(const std::string &xml);
  std::string ensureUtf8(const std::string &xml);
  std::string extractSectionTitle(const std::string &sectionXml);
  std::string stripHtmlTags(const std::string &html);
  std::unordered_map<std::string, std::shared_ptr<BookContent>> cache;
  std::vector<std::string> cacheOrder;
  mutable std::mutex cacheMutex;
  size_t maxCacheSize = 20;
  std::shared_ptr<BookContent> getOrLoad(const std::string &filePath);
  void evictIfNeeded();
};
