#include "Fb2MetadataExtractor.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iconv.h>
#include <regex>

std::string
Fb2MetadataExtractor::extractTagContent(const std::string &xml,
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

std::vector<char>
Fb2MetadataExtractor::decodeBase64(const std::string &encoded) {
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++)
    T[chars[i]] = i;
  std::vector<char> dec;
  dec.reserve(encoded.size() * 3 / 4);
  int val = 0, valb = -8;
  for (unsigned char c : encoded) {
    if (T[c] == -1)
      continue;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      dec.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return dec;
}

std::string
Fb2MetadataExtractor::detectXmlEncoding(const std::string &xmlHead) {
  std::regex encRegex(R"(encoding\s*=\s*["']([^"']+)["'])", std::regex::icase);
  std::smatch match;
  if (std::regex_search(xmlHead, match, encRegex) && match.size() > 1) {
    std::string enc = match[1].str();
    std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);
    return enc;
  }
  return "utf-8";
}

bool Fb2MetadataExtractor::isUtf8(const std::string &s) {
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

std::string
Fb2MetadataExtractor::convertToUtf8(const std::string &input,
                                    const std::string &fromEncoding) {
  if (input.empty())
    return input;
  std::string from = fromEncoding;
  std::transform(from.begin(), from.end(), from.begin(), ::tolower);
  if (from == "windows-1251" || from == "cp1251" || from == "win1251" ||
      from == "ц-1251") {
    from = "CP1251";
  } else if (from == "koi8-r" || from == "koi8r") {
    from = "KOI8-R";
  } else if (from == "utf-8" || from == "utf8") {
    return input;
  } else {
    std::transform(from.begin(), from.end(), from.begin(), ::toupper);
  }
  iconv_t cd = iconv_open("UTF-8", from.c_str());
  if (cd == (iconv_t)-1) {
    return input;
  }
  std::string output;
  output.resize(input.size() * 2);
  char *inBuf = const_cast<char *>(input.data());
  size_t inLeft = input.size();
  char *outBuf = output.data();
  size_t outLeft = output.size();
  size_t result = iconv(cd, &inBuf, &inLeft, &outBuf, &outLeft);
  iconv_close(cd);
  if (result == (size_t)-1) {
    return input;
  }
  output.resize(output.size() - outLeft);
  return output;
}

bool Fb2MetadataExtractor::extractFull(const std::string &filePath,
                                       Book &book) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;
  auto fileSize = file.tellg();
  if (fileSize <= 0)
    return false;
  file.seekg(0, std::ios::beg);
  std::string xml;
  xml.resize(static_cast<size_t>(fileSize));
  if (!file.read(xml.data(), fileSize))
    return false;
  file.close();
  std::string xmlHead = xml.substr(0, std::min<size_t>(xml.size(), 500));
  std::string encoding = detectXmlEncoding(xmlHead);
  bool needConversion = false;
  std::string utf8Xml = xml;
  std::string encLower = encoding;
  std::transform(encLower.begin(), encLower.end(), encLower.begin(), ::tolower);
  if (encLower != "utf-8" && encLower != "utf8") {
    needConversion = true;
  } else {
    if (!isUtf8(xmlHead)) {
      needConversion = true;
      encoding = "CP1251";
    }
  }
  if (needConversion) {
    utf8Xml = convertToUtf8(xml, encoding);
    if (utf8Xml.empty() && !xml.empty()) {
      utf8Xml = xml;
    }
  }
  size_t bodyPos = utf8Xml.find("<body>");
  std::string titleInfo =
      (bodyPos != std::string::npos) ? utf8Xml.substr(0, bodyPos) : utf8Xml;

  book.title = extractTagContent(titleInfo, "book-title");
  book.author = extractTagContent(titleInfo, "author");

  std::regex uuidRegex(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
  book.author = std::regex_replace(book.author, uuidRegex, "");

  std::regex urlRegex(R"(https?://[^\s]+|www\.[^\s]+)");
  book.author = std::regex_replace(book.author, urlRegex, "");

  size_t unknownPos = book.author.find("Неизвестный автор");
  if (unknownPos != std::string::npos && book.author.length() > 17) {
    book.author = book.author.substr(0, unknownPos);
  }

  std::regex extraSpaces(R"(\s+)");
  book.author = std::regex_replace(book.author, extraSpaces, " ");
  if (!book.author.empty() && book.author.front() == ' ') {
    book.author = book.author.substr(1);
  }
  if (!book.author.empty() && book.author.back() == ' ') {
    book.author.pop_back();
  }

  if (book.author.empty()) {
    book.author = "Unknown";
  }

  book.genre = extractTagContent(titleInfo, "genre");
  book.language = extractTagContent(titleInfo, "lang");
  std::regex seqRegex(
      "<sequence[^>]*name=\"([^\"]*)\"[^>]*number=\"([^\"]*)\"");
  std::smatch seqMatch;
  if (std::regex_search(titleInfo, seqMatch, seqRegex)) {
    book.sequence = seqMatch[1].str();
    try {
      book.sequenceNumber = std::stoi(seqMatch[2].str());
    } catch (...) {
    }
  }
  std::regex coverLinkRegex(
      "<coverpage>[\\s\\S]*?<image[^>]*l:href=\"#([^\"]+)\"");
  std::smatch coverMatch;
  if (std::regex_search(utf8Xml, coverMatch, coverLinkRegex)) {
    std::string coverId = coverMatch[1].str();
    std::regex binaryRegex(
        "<binary[^>]*id=\"" + coverId +
        "\"[^>]*content-type=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</binary>");
    std::smatch binMatch;
    if (std::regex_search(utf8Xml, binMatch, binaryRegex)) {
      book.coverMime = binMatch[1].str();
      std::string b64Data = binMatch[2].str();
      b64Data.erase(std::remove_if(b64Data.begin(), b64Data.end(), ::isspace),
                    b64Data.end());
      book.coverData = decodeBase64(b64Data);
    }
  }
  book.filePath = filePath;
  return !book.title.empty();
}
