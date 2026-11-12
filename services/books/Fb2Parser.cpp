#include "Fb2Parser.h"

#include <algorithm>
#include <cstring>
#include <iconv.h>

namespace {
std::string toLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), ::tolower);
  return out;
}
bool isSelfClosingTag(std::string_view tag) {
  if (tag.empty())
    return false;
  return tag.back() == '/';
}
std::string tagName(std::string_view tag) {
  size_t i = 0;
  if (i < tag.size() && tag[i] == '/')
    ++i;
  size_t start = i;
  while (i < tag.size()) {
    char c = tag[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/' || c == '>')
      break;
    ++i;
  }
  return std::string(tag.substr(start, i - start));
}
} // namespace

bool Fb2Parser::isValidUtf8(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
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
      if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80)
        return false;
    }
    i += len;
  }
  return true;
}

std::string Fb2Parser::detectXmlEncoding(std::string_view xmlHead) {
  const char *keys[] = {"encoding=", "ENCODING=", "Encoding="};
  for (const char *key : keys) {
    size_t pos = xmlHead.find(key);
    if (pos == std::string_view::npos)
      continue;
    pos += std::strlen(key);
    while (pos < xmlHead.size() &&
           (xmlHead[pos] == ' ' || xmlHead[pos] == '\t'))
      ++pos;
    if (pos >= xmlHead.size())
      continue;
    char quote = xmlHead[pos];
    if (quote != '"' && quote != '\'')
      continue;
    size_t start = pos + 1;
    size_t end = xmlHead.find(quote, start);
    if (end == std::string_view::npos)
      continue;
    return toLower(xmlHead.substr(start, end - start));
  }
  return "utf-8";
}

std::string Fb2Parser::convertToUtf8(std::string_view input,
                                     std::string_view fromEncoding) {
  if (input.empty())
    return std::string(input);
  std::string from = toLower(fromEncoding);
  if (from == "windows-1251" || from == "cp1251" || from == "win1251") {
    from = "CP1251";
  } else if (from == "koi8-r" || from == "koi8r") {
    from = "KOI8-R";
  } else if (from == "utf-8" || from == "utf8") {
    return std::string(input);
  } else {
    std::transform(from.begin(), from.end(), from.begin(), ::toupper);
  }
  iconv_t cd = iconv_open("UTF-8", from.c_str());
  if (cd == (iconv_t)-1)
    return std::string(input);
  std::string source(input);
  std::string output;
  output.resize(source.size() * 2);
  char *inBuf = source.data();
  size_t inLeft = source.size();
  char *outBuf = output.data();
  size_t outLeft = output.size();
  size_t result = iconv(cd, &inBuf, &inLeft, &outBuf, &outLeft);
  iconv_close(cd);
  if (result == (size_t)-1)
    return std::string(input);
  output.resize(output.size() - outLeft);
  return output;
}

std::string Fb2Parser::ensureUtf8(std::string_view xml) {
  std::string_view xmlHead = xml.substr(0, std::min<size_t>(xml.size(), 500));
  std::string encoding = detectXmlEncoding(xmlHead);
  std::string encLower = toLower(encoding);
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
  return std::string(xml);
}

std::string Fb2Parser::decodeBase64(std::string_view encoded) {
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static const std::vector<int> table = [] {
    std::vector<int> t(256, -1);
    for (int i = 0; i < 64; ++i)
      t[static_cast<unsigned char>(chars[i])] = i;
    return t;
  }();
  std::string dec;
  dec.reserve(encoded.size() * 3 / 4);
  int val = 0;
  int valb = -8;
  for (unsigned char c : encoded) {
    if (table[c] == -1)
      continue;
    val = (val << 6) + table[c];
    valb += 6;
    if (valb >= 0) {
      dec.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return dec;
}

std::string Fb2Parser::escapeHtml(std::string_view text) {
  std::string result;
  result.reserve(text.size() + text.size() / 8);
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

std::string Fb2Parser::stripHtmlTags(std::string_view html) {
  std::string result;
  result.reserve(html.size());
  bool inTag = false;
  bool lastWasSpace = true;
  for (char c : html) {
    if (c == '<') {
      inTag = true;
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        if (!lastWasSpace) {
          result += ' ';
          lastWasSpace = true;
        }
      } else {
        result += c;
        lastWasSpace = false;
      }
    }
  }
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}

std::string Fb2Parser::extractTagText(std::string_view xml,
                                      std::string_view tag) {
  std::string open;
  open.reserve(tag.size() + 1);
  open += '<';
  open += tag;
  size_t pos = 0;
  while ((pos = xml.find(open, pos)) != std::string_view::npos) {
    size_t tagEnd = xml.find('>', pos);
    if (tagEnd == std::string_view::npos)
      return "";
    if (pos + open.size() < xml.size()) {
      char next = xml[pos + open.size()];
      if (next != '>' && next != ' ' && next != '\t' && next != '\n' &&
          next != '\r' && next != '/') {
        pos = tagEnd + 1;
        continue;
      }
    }
    std::string close;
    close.reserve(tag.size() + 3);
    close += "</";
    close += tag;
    close += '>';
    size_t end = xml.find(close, tagEnd);
    if (end == std::string_view::npos)
      return "";
    std::string_view inner = xml.substr(tagEnd + 1, end - tagEnd - 1);
    std::string text = stripHtmlTags(inner);
    if (text.find('<') != std::string::npos)
      text = stripHtmlTags(text);
    return text;
  }
  return "";
}

Fb2Parser::TagSpan Fb2Parser::readTag(std::string_view s, size_t pos) {
  TagSpan span{pos, pos, std::string(), false, false};
  size_t gt = s.find('>', pos);
  if (gt == std::string_view::npos) {
    span.end = s.size();
    return span;
  }
  span.end = gt + 1;
  std::string_view inner = s.substr(pos + 1, gt - pos - 1);
  if (!inner.empty() && inner[0] == '/') {
    span.closing = true;
    inner.remove_prefix(1);
  }
  span.selfClosing = isSelfClosingTag(inner);
  span.name = tagName(inner);
  return span;
}

std::string Fb2Parser::extractSectionTitle(std::string_view sectionXml) {
  size_t titleOpen = sectionXml.find("<title");
  if (titleOpen == std::string_view::npos)
    return "";
  size_t titleTagEnd = sectionXml.find('>', titleOpen);
  if (titleTagEnd == std::string_view::npos)
    return "";
  size_t titleClose = sectionXml.find("</title>", titleTagEnd);
  if (titleClose == std::string_view::npos)
    return "";
  std::string_view inner =
      sectionXml.substr(titleTagEnd + 1, titleClose - titleTagEnd - 1);
  return extractTagText(inner, "p");
}

std::string Fb2Parser::sectionToHtml(std::string_view xml) {
  size_t contentStart = xml.find('>');
  if (contentStart == std::string_view::npos)
    return "";
  ++contentStart;
  size_t contentEnd = xml.rfind("</section>");
  if (contentEnd == std::string_view::npos)
    contentEnd = xml.size();
  std::string out;
  out.reserve(contentEnd - contentStart + 64);
  size_t i = contentStart;
  while (i < contentEnd) {
    char c = xml[i];
    if (c != '<') {
      out += c;
      ++i;
      continue;
    }
    TagSpan tag = readTag(xml, i);
    if (tag.end > contentEnd) {
      out.append(xml.substr(i, contentEnd - i));
      break;
    }
    const std::string &n = tag.name;
    if (tag.closing) {
      if (n == "title")
        out += "</h2>";
      else if (n == "subtitle")
        out += "</h3>";
      else if (n == "cite")
        out += "</blockquote>";
      else if (n == "poem")
        out += "</pre>";
      else if (n == "stanza")
        out += "<br/>";
      else if (n == "v")
        out += "<br/>";
      else if (n == "emphasis")
        out += "</em>";
      else if (n == "text-author")
        out += "</p>";
      else if (n == "epigraph")
        out += "</div>";
      else if (n == "section")
        ;
      else if (n == "a")
        out += "</a>";
      else if (n == "p")
        out += "</p>";
    } else if (tag.selfClosing) {
      if (n == "empty-line")
        out += "<br/>";
    } else {
      if (n == "title") {
        out += "<h2>";
        size_t close = xml.find("</title>", tag.end);
        if (close != std::string_view::npos) {
          std::string_view inner = xml.substr(tag.end, close - tag.end);
          std::string text = extractTagText(inner, "p");
          out += escapeHtml(text);
          out += "</h2>";
          i = close + 8;
          continue;
        }
      } else if (n == "subtitle") {
        out += "<h3>";
      } else if (n == "cite") {
        out += "<blockquote>";
      } else if (n == "poem") {
        out += "<pre class=\"poem\">";
      } else if (n == "stanza") {
      } else if (n == "v") {
      } else if (n == "emphasis") {
        out += "<em>";
      } else if (n == "text-author") {
        out += "<p class=\"author\">";
      } else if (n == "epigraph") {
        out += "<div class=\"epigraph\">";
      } else if (n == "section") {
      } else if (n == "a") {
        std::string_view inner = xml.substr(i + 1, tag.end - i - 2);
        size_t hrefPos = inner.find("l:href=\"");
        if (hrefPos != std::string_view::npos) {
          hrefPos += 8;
          size_t hrefEnd = inner.find('"', hrefPos);
          if (hrefEnd != std::string_view::npos) {
            std::string href(inner.substr(hrefPos, hrefEnd - hrefPos));
            out += "<a href=\"" + href + "\">";
          }
        }
      } else if (n == "p") {
        out += "<p>";
      }
    }
    i = tag.end;
  }
  return out;
}

std::vector<Fb2Parser::SectionRange>
Fb2Parser::findSections(std::string_view xml, size_t bodyStart,
                        size_t bodyEnd) {
  std::vector<SectionRange> result;
  size_t searchPos = bodyStart;
  while (searchPos < bodyEnd) {
    size_t sectionStart = xml.find("<section", searchPos);
    if (sectionStart == std::string_view::npos || sectionStart >= bodyEnd)
      break;
    size_t tagEnd = xml.find('>', sectionStart);
    if (tagEnd == std::string_view::npos)
      break;
    int depth = 1;
    size_t pos = tagEnd + 1;
    size_t sectionEnd = std::string_view::npos;
    while (pos < bodyEnd && depth > 0) {
      size_t nextOpen = xml.find("<section", pos);
      size_t nextClose = xml.find("</section>", pos);
      if (nextClose == std::string_view::npos || nextClose >= bodyEnd) {
        if (nextOpen != std::string_view::npos && nextOpen < bodyEnd) {
          depth++;
          pos = xml.find('>', nextOpen) + 1;
          continue;
        }
        sectionEnd = bodyEnd;
        break;
      }
      if (nextOpen != std::string_view::npos && nextOpen < nextClose &&
          nextOpen < bodyEnd) {
        depth++;
        pos = xml.find('>', nextOpen) + 1;
      } else {
        depth--;
        if (depth == 0) {
          sectionEnd = nextClose + 10;
          break;
        }
        pos = nextClose + 10;
      }
    }
    if (sectionEnd == std::string_view::npos || sectionEnd > bodyEnd)
      sectionEnd = bodyEnd;
    result.push_back({sectionStart, sectionEnd});
    searchPos = sectionEnd;
  }
  return result;
}
