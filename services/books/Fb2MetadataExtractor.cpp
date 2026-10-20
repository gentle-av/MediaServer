#include "Fb2MetadataExtractor.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

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
  int val = 0, valb = -8;
  for (unsigned char c : encoded) {
    if (T[c] == -1)
      break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      dec.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return dec;
}

bool Fb2MetadataExtractor::extractMetadata(const std::string &filePath,
                                           Book &book) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open())
    return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string xml = buffer.str();
  size_t bodyPos = xml.find("<body>");
  if (bodyPos != std::string::npos) {
    xml = xml.substr(0, bodyPos + 1000);
  }
  book.title = extractTagContent(xml, "book-title");
  std::string author = extractTagContent(xml, "author");
  book.author = author;
  book.genre = extractTagContent(xml, "genre");
  book.language = extractTagContent(xml, "lang");
  book.sequence = extractTagContent(xml, "sequence");
  std::string seqNumStr = extractTagContent(xml, "sequence");
  std::regex seqRegex(
      "<sequence[^>]*name=\"([^\"]*)\"[^>]*number=\"([^\"]*)\"");
  std::smatch seqMatch;
  if (std::regex_search(xml, seqMatch, seqRegex)) {
    book.sequence = seqMatch[1].str();
    try {
      book.sequenceNumber = std::stoi(seqMatch[2].str());
    } catch (...) {
    }
  }

  std::regex coverLinkRegex("<coverpage>\\s*<image[^>]*l:href=\"#([^\"]+)\"");
  std::smatch coverMatch;
  if (std::regex_search(xml, coverMatch, coverLinkRegex)) {
    std::string coverId = coverMatch[1].str();
    std::regex binaryRegex(
        "<binary[^>]*id=\"" + coverId +
        "\"[^>]*content-type=\"([^\"]+)\"[^>]*>([^<]+)</binary>");
    std::smatch binMatch;
    std::ifstream fullFile(filePath, std::ios::binary);
    std::stringstream fullBuf;
    fullBuf << fullFile.rdbuf();
    std::string fullXml = fullBuf.str();
    if (std::regex_search(fullXml, binMatch, binaryRegex)) {
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
