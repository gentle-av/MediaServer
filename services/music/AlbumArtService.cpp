#include "services/music/AlbumArtService.h"
#include "MetadataExtractor.h"

AlbumArtService::AlbumArtService(MusicDatabase &db) : db_(db) {}

std::string AlbumArtService::detectMimeType(const std::vector<char> &data) {
  if (data.size() >= 8) {
    if (data[0] == 0xFF && data[1] == 0xD8)
      return "image/jpeg";
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E &&
        data[3] == 0x47)
      return "image/png";
    if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46)
      return "image/gif";
  }
  return "application/octet-stream";
}

AlbumArtService::AlbumArt
AlbumArtService::getAlbumArt(const std::string &filePath) {
  AlbumArt result;
  auto dbArt = db_.getAlbumArt(filePath);
  result.data = dbArt.data;
  result.mimeType =
      dbArt.mimeType.empty() ? detectMimeType(dbArt.data) : dbArt.mimeType;
  return result;
}

AlbumArtService::AlbumArt
AlbumArtService::getAlbumArtByAlbum(const std::string &album,
                                    const std::string &artist) {
  AlbumArt result;
  std::string filePath = db_.getFilePathByAlbum(album, artist);
  if (!filePath.empty()) {
    result = getAlbumArt(filePath);
  }
  return result;
}

bool AlbumArtService::saveAlbumArt(const std::string &filePath,
                                   const std::vector<char> &data) {
  return db_.saveAlbumArt(filePath, data);
}

drogon::HttpResponsePtr
AlbumArtService::createImageResponse(const AlbumArt &albumArt) {
  if (albumArt.data.empty()) {
    return drogon::HttpResponse::newNotFoundResponse();
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  if (albumArt.mimeType == "image/jpeg") {
    resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
  } else if (albumArt.mimeType == "image/png") {
    resp->setContentTypeCode(drogon::CT_IMAGE_PNG);
  } else if (albumArt.mimeType == "image/gif") {
    resp->setContentTypeCode(drogon::CT_IMAGE_GIF);
  } else {
    resp->setContentTypeCode(drogon::CT_APPLICATION_OCTET_STREAM);
  }
  resp->setBody(std::string(albumArt.data.data(), albumArt.data.size()));
  return resp;
}
