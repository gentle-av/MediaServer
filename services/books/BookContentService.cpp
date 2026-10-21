#include "BookContentService.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iconv.h>
#include <iostream>
#include <regex>

namespace {
bool isValidUtf8(const std::string &s) {
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = s[i];
    size_t len = 0;
    if (c < 0x80)
      len = 1;
    else if ((c >> 5) == 0x6)
      len = 2;
    else if ((c >> 4) == 0xE)
      len = 3;
    else if ((c >> 3) == 0x1E)
      len = 4;
    else
      return false;
    if (i + len > s.size())
      return false;
    for (size_t j = 1; j < len; ++j) {
      if ((s[i + j] & 0xC0) != 0x80)
        return false;
    }
    i += len;
  }
  return true;
}
std::string detectXmlEncoding(const std::string &xmlHead) {
  std::regex encRegex(R"(encoding\s*=\s*["']([^"']+)["'])", std::regex::icase);
  std::smatch match;
  if (std::regex_search(xmlHead, match, encRegex) && match.size() > 1) {
    std::string enc = match[1].str();
    std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);
    return enc;
  }
  return "utf-8";
}
std::string convertToUtf8(const std::string &input,
                          const std::string &fromEncoding) {
  if (input.empty())
    return input;
  std::string from = fromEncoding;
  std::transform(from.begin(), from.end(), from.begin(), ::tolower);
  if (from == "windows-1251" || from == "cp1251" || from == "win1251") {
    from = "CP1251";
  } else if (from == "koi8-r" || from == "koi8r") {
    from = "KOI8-R";
  } else if (from == "utf-8" || from == "utf8") {
    return input;
  } else {
    std::transform(from.begin(), from.end(), from.begin(), ::toupper);
  }
  iconv_t cd = iconv_open("UTF-8", from.c_str());
  if (cd == (iconv_t)-1)
    return input;
  std::string output;
  output.resize(input.size() * 2);
  char *inBuf = const_cast<char *>(input.data());
  size_t inLeft = input.size();
  char *outBuf = output.data();
  size_t outLeft = output.size();
  size_t result = iconv(cd, &inBuf, &inLeft, &outBuf, &outLeft);
  iconv_close(cd);
  if (result == (size_t)-1)
    return input;
  output.resize(output.size() - outLeft);
  return output;
}
std::string escapeHtml(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (char c : text) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&#39;";
      break;
    default:
      result += c;
    }
  }
  return result;
}
std::string stripHtmlTags(const std::string &html) {
  std::string result;
  result.reserve(html.size());
  bool inTag = false;
  for (char c : html) {
    if (c == '<') {
      inTag = true;
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      result += c;
    }
  }
  std::regex spaces("\\s+");
  result = std::regex_replace(result, spaces, " ");
  if (!result.empty() && result[0] == ' ')
    result = result.substr(1);
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}
std::string extractTagContent(const std::string &xml,
                              const std::string &tagName) {
  std::regex tagRegex("<" + tagName + "[^>]*>([\\s\\S]*?)</" + tagName + ">",
                      std::regex::icase);
  std::smatch match;
  if (std::regex_search(xml, match, tagRegex) && match.size() > 1) {
    std::string content = match.str(1);
    std::regex innerTags("<[^>]+>");
    content = std::regex_replace(content, innerTags, " ");
    std::regex spaces("\\s+");
    content = std::regex_replace(content, spaces, " ");
    if (!content.empty() && content[0] == ' ')
      content = content.substr(1);
    return content;
  }
  return "";
}
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
      if (orderIt != cacheOrder.end()) {
        cacheOrder.erase(orderIt);
      }
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
    const std::string &oldest = cacheOrder.front();
    cache.erase(oldest);
    cacheOrder.erase(cacheOrder.begin());
  }
}

std::shared_ptr<BookContent>
BookContentService::parseFb2(const std::string &filePath) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "[BookContentService] Cannot open file: " << filePath
              << std::endl;
    return nullptr;
  }
  auto fileSize = file.tellg();
  if (fileSize <= 0)
    return nullptr;
  file.seekg(0, std::ios::beg);
  std::string xml;
  xml.resize(static_cast<size_t>(fileSize));
  if (!file.read(xml.data(), fileSize))
    return nullptr;
  file.close();
  xml = ensureUtf8(xml);
  auto content = std::make_shared<BookContent>();
  content->filePath = filePath;
  size_t bodyPos = xml.find("<body>");
  std::string titleInfo =
      (bodyPos != std::string::npos) ? xml.substr(0, bodyPos) : xml;
  content->title = extractTagContent(titleInfo, "book-title");
  content->author = extractTagContent(titleInfo, "author");
  content->language = extractTagContent(titleInfo, "lang");
  if (bodyPos == std::string::npos) {
    content->parsed = true;
    return content;
  }
  size_t bodyEnd = xml.find("</body>", bodyPos);
  if (bodyEnd == std::string::npos) {
    bodyEnd = xml.size();
  }
  std::string body = xml.substr(bodyPos, bodyEnd - bodyPos);
  size_t searchPos = 0;
  int sectionIndex = 0;
  size_t currentCharOffset = 0;
  while (searchPos < body.size()) {
    size_t sectionStart = body.find("<section", searchPos);
    if (sectionStart == std::string::npos)
      break;
    size_t tagEnd = body.find('>', sectionStart);
    if (tagEnd == std::string::npos)
      break;
    int depth = 1;
    size_t pos = tagEnd + 1;
    while (pos < body.size() && depth > 0) {
      size_t nextOpen = body.find("<section", pos);
      size_t nextClose = body.find("</section>", pos);
      if (nextClose == std::string::npos)
        break;
      if (nextOpen != std::string::npos && nextOpen < nextClose) {
        depth++;
        pos = body.find('>', nextOpen) + 1;
      } else {
        depth--;
        if (depth == 0) {
          size_t sectionEnd = nextClose + 10;
          std::string sectionXml =
              body.substr(sectionStart, sectionEnd - sectionStart);
          std::string html = fb2SectionToHtml(sectionXml);
          std::string title = extractSectionTitle(sectionXml);
          std::string plainText = stripHtmlTags(html);
          size_t charLength = plainText.size();
          BookSection section;
          section.index = sectionIndex++;
          section.depth = 0;
          section.title = title;
          section.html = std::move(html);
          section.charOffset = currentCharOffset;
          section.charLength = charLength;
          content->sections.push_back(std::move(section));
          currentCharOffset += charLength;
          searchPos = sectionEnd;
          break;
        }
        pos = nextClose + 10;
      }
    }
    if (depth != 0) {
      searchPos = tagEnd + 1;
    }
  }
  content->totalChars = currentCharOffset;
  content->parsed = true;
  return content;
}

std::string BookContentService::ensureUtf8(const std::string &xml) {
  std::string xmlHead = xml.substr(0, std::min<size_t>(xml.size(), 500));
  std::string encoding = detectXmlEncoding(xmlHead);
  std::string encLower = encoding;
  std::transform(encLower.begin(), encLower.end(), encLower.begin(), ::tolower);
  bool needConversion = false;
  if (encLower != "utf-8" && encLower != "utf8") {
    needConversion = true;
  } else {
    if (!isValidUtf8(xmlHead)) {
      needConversion = true;
      encoding = "CP1251";
    }
  }
  if (needConversion) {
    std::string utf8Xml = convertToUtf8(xml, encoding);
    if (!utf8Xml.empty())
      return utf8Xml;
  }
  return xml;
}

std::string
BookContentService::extractSectionTitle(const std::string &sectionXml) {
  std::regex titleRegex("<title[^>]*>([\\s\\S]*?)</title>", std::regex::icase);
  std::smatch match;
  if (std::regex_search(sectionXml, match, titleRegex) && match.size() > 1) {
    std::string titleXml = match.str(1);
    return extractTagContent(titleXml, "p");
  }
  return "";
}

std::string BookContentService::fb2SectionToHtml(const std::string &xml) {
  std::string html;
  html.reserve(xml.size());
  size_t contentStart = xml.find('>') + 1;
  size_t contentEnd = xml.rfind("</section>");
  if (contentEnd == std::string::npos)
    contentEnd = xml.size();
  std::string content = xml.substr(contentStart, contentEnd - contentStart);
  std::regex titleRegex(R"(<title[^>]*>([\s\S]*?)</title>)", std::regex::icase);
  content = std::regex_replace(
      content, titleRegex, [](const std::smatch &m) -> std::string {
        std::string inner = m.str(1);
        std::regex pRegex(R"(<p[^>]*>([\s\S]*?)</p>)", std::regex::icase);
        std::string titleText;
        std::sregex_iterator it(inner.begin(), inner.end(), pRegex);
        std::sregex_iterator end;
        for (; it != end; ++it) {
          if (!titleText.empty())
            titleText += " ";
          titleText += (*it)[1].str();
        }
        std::regex innerTags("<[^>]+>");
        titleText = std::regex_replace(titleText, innerTags, "");
        return "<h2>" + titleText + "</h2>";
      });
  std::regex emptyLineRegex(R"(<empty-line\s*/?>)", std::regex::icase);
  content = std::regex_replace(content, emptyLineRegex, "<br/>");
  std::regex citeOpenRegex(R"(<cite[^>]*>)", std::regex::icase);
  std::regex citeCloseRegex(R"(</cite>)", std::regex::icase);
  content = std::regex_replace(content, citeOpenRegex, "<blockquote>");
  content = std::regex_replace(content, citeCloseRegex, "</blockquote>");
  std::regex poemOpenRegex(R"(<poem[^>]*>)", std::regex::icase);
  std::regex poemCloseRegex(R"(</poem>)", std::regex::icase);
  content = std::regex_replace(content, poemOpenRegex, R"(<pre class="poem">)");
  content = std::regex_replace(content, poemCloseRegex, "</pre>");
  std::regex stanzaOpenRegex(R"(<stanza[^>]*>)", std::regex::icase);
  std::regex stanzaCloseRegex(R"(</stanza>)", std::regex::icase);
  content = std::regex_replace(content, stanzaOpenRegex, "");
  content = std::regex_replace(content, stanzaCloseRegex, "<br/>");
  std::regex vOpenRegex(R"(<v[^>]*>)", std::regex::icase);
  std::regex vCloseRegex(R"(</v>)", std::regex::icase);
  content = std::regex_replace(content, vOpenRegex, "");
  content = std::regex_replace(content, vCloseRegex, "<br/>");
  std::regex emphasisOpenRegex(R"(<emphasis[^>]*>)", std::regex::icase);
  std::regex emphasisCloseRegex(R"(</emphasis>)", std::regex::icase);
  content = std::regex_replace(content, emphasisOpenRegex, "<em>");
  content = std::regex_replace(content, emphasisCloseRegex, "</em>");
  std::regex linkRegex(
      R"(<a[^>]*l:href="([^"]*)"[^> ] * > ([\s\S] * ?) < / a >) ",
      std::regex::icase);
  content = std::regex_replace(
      content, linkRegex, [](const std::smatch &m) -> std::string {
        std::string href = m.str(1);
        std::string text = m.str(2);
        return "<a href=\"" + href + "\">" + text + "</a>";
      });
  std::regex subtitleOpenRegex(R"(<subtitle[^>]*>)", std::regex::icase);
  std::regex subtitleCloseRegex(R"(</subtitle>)", std::regex::icase);
  content = std::regex_replace(content, subtitleOpenRegex, "<h3>");
  content = std::regex_replace(content, subtitleCloseRegex, "</h3>");
  std::regex authorOpenRegex(R"(<text-author[^>]*>)", std::regex::icase);
  std::regex authorCloseRegex(R"(</text-author>)", std::regex::icase);
  content =
      std::regex_replace(content, authorOpenRegex, R"(<p class="author">)");
  content = std::regex_replace(content, authorCloseRegex, "</p>");
  std::regex epigraphOpenRegex(R"(<epigraph[^>]*>)", std::regex::icase);
  std::regex epigraphCloseRegex(R"(</epigraph>)", std::regex::icase);
  content = std::regex_replace(content, epigraphOpenRegex,
                               R"(<div class="epigraph">)");
  content = std::regex_replace(content, epigraphCloseRegex, "</div>");
  std::regex sectionOpenRegex(R"(<section[^>]*>)", std::regex::icase);
  std::regex sectionCloseRegex(R"(</section>)", std::regex::icase);
  content = std::regex_replace(content, sectionOpenRegex, "");
  content = std::regex_replace(content, sectionCloseRegex, "");
  return content;
}

std::string BookContentService::stripHtmlTags(const std::string &html) {
  return ::stripHtmlTags(html);
}
