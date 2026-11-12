#include "BookContentService.h"
#include "Fb2Parser.h"
#include <algorithm>
#include <fstream>

BookContentService &BookContentService::getInstance() {
  static BookContentService instance;
  return instance;
}

std::optional<BookContent>
BookContentService::getContent(const std::string &filePath) {
  auto content = getOrLoad(filePath);
  if (!content)
    return std::nullopt;
  return *content;
}

std::optional<BookSection>
BookContentService::getSection(const std::string &filePath, int sectionIndex) {
  auto content = getOrLoad(filePath);
  if (!content)
    return std::nullopt;
  if (sectionIndex < 0 ||
      sectionIndex >= static_cast<int>(content->sections.size())) {
    return std::nullopt;
  }
  return content->sections[sectionIndex];
}

std::optional<BookSection>
BookContentService::getSectionByCharOffset(const std::string &filePath,
                                           size_t offset) {
  auto content = getOrLoad(filePath);
  if (!content)
    return std::nullopt;
  for (const auto &section : content->sections) {
    if (offset >= section.charOffset &&
        offset < section.charOffset + section.charLength) {
      return section;
    }
  }
  if (!content->sections.empty() &&
      offset >= content->sections.back().charOffset) {
    return content->sections.back();
  }
  return std::nullopt;
}

std::optional<BookSection>
BookContentService::getSectionByProgress(const std::string &filePath,
                                         double progress) {
  auto content = getOrLoad(filePath);
  if (!content || content->totalChars == 0)
    return std::nullopt;
  if (progress < 0.0)
    progress = 0.0;
  if (progress > 1.0)
    progress = 1.0;
  size_t targetOffset = static_cast<size_t>(progress * content->totalChars);
  return getSectionByCharOffset(filePath, targetOffset);
}

std::vector<std::pair<int, std::string>>
BookContentService::getTableOfContents(const std::string &filePath) {
  auto content = getOrLoad(filePath);
  if (!content)
    return {};
  std::vector<std::pair<int, std::string>> toc;
  toc.reserve(content->sections.size());
  for (const auto &section : content->sections) {
    toc.emplace_back(section.index, section.title);
  }
  return toc;
}

void BookContentService::invalidateCache(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(cacheMutex);
  cache.erase(filePath);
  cacheOrder.erase(std::remove(cacheOrder.begin(), cacheOrder.end(), filePath),
                   cacheOrder.end());
}

void BookContentService::invalidateAllCache() {
  std::lock_guard<std::mutex> lock(cacheMutex);
  cache.clear();
  cacheOrder.clear();
}

size_t BookContentService::getCacheSize() const {
  std::lock_guard<std::mutex> lock(cacheMutex);
  return cache.size();
}

void BookContentService::setMaxCacheSize(size_t maxSize) {
  std::lock_guard<std::mutex> lock(cacheMutex);
  maxCacheSize = maxSize;
  evictIfNeeded();
}

std::shared_ptr<BookContent>
BookContentService::getOrLoad(const std::string &filePath) {
  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = cache.find(filePath);
    if (it != cache.end()) {
      auto orderIt = std::find(cacheOrder.begin(), cacheOrder.end(), filePath);
      if (orderIt != cacheOrder.end())
        cacheOrder.erase(orderIt);
      cacheOrder.push_back(filePath);
      return it->second;
    }
  }
  auto content = parseFb2(filePath);
  if (!content)
    return nullptr;
  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache[filePath] = content;
    cacheOrder.push_back(filePath);
    evictIfNeeded();
  }
  return content;
}

void BookContentService::evictIfNeeded() {
  while (cache.size() > maxCacheSize && !cacheOrder.empty()) {
    const std::string oldest = cacheOrder.front();
    cache.erase(oldest);
    cacheOrder.erase(cacheOrder.begin());
  }
}

std::shared_ptr<BookContent>
BookContentService::parseFb2(const std::string &filePath) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return nullptr;
  auto fileSize = file.tellg();
  if (fileSize <= 0)
    return nullptr;
  file.seekg(0, std::ios::beg);
  std::string xml;
  xml.resize(static_cast<size_t>(fileSize));
  if (!file.read(xml.data(), fileSize))
    return nullptr;
  file.close();
  xml = Fb2Parser::ensureUtf8(xml);
  auto content = std::make_shared<BookContent>();
  content->filePath = filePath;
  size_t bodyPos = xml.find("<body");
  std::string_view titleInfo = (bodyPos != std::string::npos)
                                   ? std::string_view(xml).substr(0, bodyPos)
                                   : std::string_view(xml);
  content->title = Fb2Parser::extractTagText(titleInfo, "book-title");
  content->author = Fb2Parser::extractTagText(titleInfo, "author");
  content->language = Fb2Parser::extractTagText(titleInfo, "lang");
  if (bodyPos == std::string::npos) {
    content->parsed = true;
    return content;
  }
  size_t bodyTagEnd = xml.find('>', bodyPos);
  if (bodyTagEnd == std::string::npos) {
    content->parsed = true;
    return content;
  }
  size_t bodyEnd = xml.find("</body>", bodyTagEnd);
  if (bodyEnd == std::string::npos)
    bodyEnd = xml.size();
  auto ranges = Fb2Parser::findSections(xml, bodyTagEnd + 1, bodyEnd);
  content->sections.reserve(ranges.size());
  int sectionIndex = 0;
  size_t currentCharOffset = 0;
  for (const auto &range : ranges) {
    std::string_view sectionView(xml.data() + range.start,
                                 range.end - range.start);
    std::string html = Fb2Parser::sectionToHtml(sectionView);
    std::string title = Fb2Parser::extractSectionTitle(sectionView);
    std::string plainText = Fb2Parser::stripHtmlTags(html);
    size_t charLength = plainText.size();
    BookSection section;
    section.index = sectionIndex++;
    section.depth = 0;
    section.title = std::move(title);
    section.html = std::move(html);
    section.charOffset = currentCharOffset;
    section.charLength = charLength;
    content->sections.push_back(std::move(section));
    currentCharOffset += charLength;
  }
  content->totalChars = currentCharOffset;
  content->parsed = true;
  return content;
}
