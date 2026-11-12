#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class Fb2Parser {
public:
  static bool isValidUtf8(std::string_view s);
  static std::string detectXmlEncoding(std::string_view xmlHead);
  static std::string convertToUtf8(std::string_view input,
                                   std::string_view fromEncoding);
  static std::string ensureUtf8(std::string_view xml);
  static std::string decodeBase64(std::string_view encoded);
  static std::string escapeHtml(std::string_view text);
  static std::string stripHtmlTags(std::string_view html);
  static std::string extractTagText(std::string_view xml, std::string_view tag);
  struct TagSpan {
    size_t start;
    size_t end;
    std::string name;
    bool closing;
    bool selfClosing;
  };
  static TagSpan readTag(std::string_view s, size_t pos);
  static std::string extractSectionTitle(std::string_view sectionXml);
  static std::string sectionToHtml(std::string_view sectionXml);
  struct SectionRange {
    size_t start;
    size_t end;
  };
  static std::vector<SectionRange>
  findSections(std::string_view xml, size_t bodyStart, size_t bodyEnd);
};
