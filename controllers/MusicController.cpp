#include "controllers/MusicController.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <json/json.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <unordered_set>

namespace fs = std::filesystem;

MusicController::MusicController() {
  const char *home = getenv("HOME");
  std::string dbPath =
      home ? std::string(home) + "/.local/share/media-explorer/music.db"
           : "./music.db";
  fs::create_directories(fs::path(dbPath).parent_path());
  db_ = std::make_unique<MusicDatabase>(dbPath);
  db_->init();
  musicDir_ = "/mnt/media/music";
  if (!fs::exists(musicDir_))
    musicDir_ = "./music";
  scanNewFiles();
  removeMissingFiles();
}

void MusicController::listFiles(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto allFiles = db_->getAllFiles();
    Json::Value files(Json::arrayValue);
    for (const auto &filePath : allFiles) {
      if (fs::exists(filePath)) {
        Json::Value fileInfo;
        fileInfo["path"] = filePath;
        fileInfo["filename"] = fs::path(filePath).filename().string();
        MusicMetadata metadata;
        if (db_->getMetadata(filePath, metadata)) {
          fileInfo["title"] = metadata.title;
          fileInfo["artist"] = metadata.artist;
          fileInfo["album"] = metadata.album;
          fileInfo["duration"] = metadata.duration;
          fileInfo["track"] = metadata.track;
          fileInfo["year"] = metadata.year;
          fileInfo["genre"] = metadata.genre;
        }
        files.append(fileInfo);
      } else {
        db_->removeFile(filePath);
      }
    }
    response["status"] = "success";
    response["files"] = files;
    response["count"] = static_cast<int>(files.size());
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getArtists(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto artists = db_->getArtists();
    Json::Value artistsJson(Json::arrayValue);
    for (const auto &artist : artists) {
      artistsJson.append(artist);
    }
    response["status"] = "success";
    response["artists"] = artistsJson;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getAlbums(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    auto albums = db_->getAlbums();
    Json::Value albumsJson(Json::arrayValue);
    for (const auto &[album, artist, year] : albums) {
      Json::Value albumObj;
      albumObj["album"] = album;
      albumObj["artist"] = artist;
      albumObj["year"] = year;
      albumsJson.append(albumObj);
    }
    response["status"] = "success";
    response["albums"] = albumsJson;
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getAlbumArt(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &path) {
  std::string decodedPath = drogon::utils::urlDecode(path);
  auto albumArt = db_->getAlbumArt(decodedPath);
  if (albumArt.data.empty()) {
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    callback(resp);
    return;
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
  resp->setBody(std::string(albumArt.data.data(), albumArt.data.size()));
  callback(resp);
}

void MusicController::scan(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    scanNewFiles();
    response["status"] = "success";
    response["message"] = "Scan completed";
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::removeMissing(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  Json::Value response;
  try {
    removeMissingFiles();
    response["status"] = "success";
    response["message"] = "Missing files removed";
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::scanNewFiles() {
  auto dbFiles = db_->getAllFiles();
  std::unordered_set<std::string> existingFiles(dbFiles.begin(), dbFiles.end());
  std::vector<fs::path> musicFiles;
  if (fs::exists(musicDir_)) {
    for (const auto &entry : fs::recursive_directory_iterator(musicDir_)) {
      if (entry.is_regular_file()) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mp3" || ext == ".flac" || ext == ".m4a" || ext == ".wav") {
          musicFiles.push_back(entry.path());
        }
      }
    }
  }
  for (const auto &filePath : musicFiles) {
    std::string pathStr = filePath.string();
    if (existingFiles.find(pathStr) == existingFiles.end()) {
      MusicMetadata metadata;
      if (extractMetadata(pathStr, metadata)) {
        if (db_->addFile(pathStr, metadata)) {
          LOG_INFO << "Added new file: " << pathStr;
          std::vector<char> albumArt;
          if (extractAlbumArt(pathStr, albumArt)) {
            db_->saveAlbumArt(pathStr, albumArt);
          }
        }
      }
    }
  }
}

void MusicController::removeMissingFiles() {
  auto allFiles = db_->getAllFiles();
  for (const auto &filePath : allFiles) {
    if (!fs::exists(filePath)) {
      db_->removeFile(filePath);
      LOG_INFO << "Removed missing file: " << filePath;
    }
  }
}

bool MusicController::extractMetadata(const std::string &filePath,
                                      MusicMetadata &metadata) {
  if (!fs::exists(filePath))
    return false;
  try {
    TagLib::FileRef f(filePath.c_str());
    if (!f.isNull() && f.tag()) {
      TagLib::Tag *tag = f.tag();
      metadata.title = tag->title().to8Bit(true);
      metadata.artist = tag->artist().to8Bit(true);
      metadata.album = tag->album().to8Bit(true);
      if (f.audioProperties()) {
        metadata.duration = f.audioProperties()->lengthInSeconds();
      } else {
        metadata.duration = 0;
      }
      metadata.track = tag->track();
      metadata.year = tag->year();
      metadata.genre = tag->genre().to8Bit(true);
      return true;
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting metadata: " << e.what();
  }
  metadata.title = fs::path(filePath).stem().string();
  metadata.artist = "Unknown";
  metadata.album = "Unknown";
  return true;
}

bool MusicController::extractAlbumArt(const std::string &filePath,
                                      std::vector<char> &albumArt) {
  LOG_INFO << "Extracting album art from: " << filePath;
  try {
    std::string ext = filePath.substr(filePath.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".mp3") {
      TagLib::MPEG::File mp3File(filePath.c_str());
      TagLib::ID3v2::Tag *tag = mp3File.ID3v2Tag();
      if (tag) {
        TagLib::ID3v2::FrameList frames = tag->frameList("APIC");
        LOG_INFO << "Found " << frames.size() << " APIC frames";
        if (!frames.isEmpty()) {
          TagLib::ID3v2::AttachedPictureFrame *frame =
              static_cast<TagLib::ID3v2::AttachedPictureFrame *>(
                  frames.front());
          if (frame && frame->picture().size() > 0) {
            albumArt.assign(frame->picture().data(),
                            frame->picture().data() + frame->picture().size());
            LOG_INFO << "Extracted MP3 album art, size: " << albumArt.size();
            return true;
          }
        }
      }
    } else if (ext == ".flac") {
      TagLib::FLAC::File flacFile(filePath.c_str());
      if (flacFile.isOpen()) {
        auto pictures = flacFile.pictureList();
        LOG_INFO << "Found " << pictures.size() << " FLAC pictures";
        if (!pictures.isEmpty()) {
          TagLib::FLAC::Picture *bestPicture = nullptr;
          for (auto it = pictures.begin(); it != pictures.end(); ++it) {
            TagLib::FLAC::Picture *picture = *it;
            if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
              bestPicture = picture;
              break;
            }
            if (!bestPicture)
              bestPicture = picture;
          }
          if (bestPicture) {
            TagLib::ByteVector data = bestPicture->data();
            albumArt.assign(data.data(), data.data() + data.size());
            LOG_INFO << "Extracted FLAC album art, size: " << albumArt.size();
            return true;
          }
        }
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error extracting album art from " << filePath << ": "
              << e.what();
  }
  return false;
}

void MusicController::getTracksByArtist(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &artist) {
  Json::Value response;
  try {
    std::string decodedArtist = drogon::utils::urlDecode(artist);
    auto tracks = db_->getTracksByArtist(decodedArtist);
    Json::Value tracksJson(Json::arrayValue);
    for (const auto &track : tracks) {
      Json::Value trackObj;
      trackObj["path"] = track.filePath;
      trackObj["title"] = track.title;
      trackObj["artist"] = track.artist;
      trackObj["album"] = track.album;
      trackObj["duration"] = track.duration;
      trackObj["track"] = track.track;
      trackObj["year"] = track.year;
      trackObj["genre"] = track.genre;
      tracksJson.append(trackObj);
    }
    response["status"] = "success";
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracks.size());
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}

void MusicController::getTracksByAlbum(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &album) {
  Json::Value response;
  try {
    std::string decodedAlbum = drogon::utils::urlDecode(album);
    fs::path albumPath;
    for (const auto &artistDir : fs::directory_iterator(musicDir_)) {
      if (artistDir.is_directory()) {
        for (const auto &albumDir : fs::directory_iterator(artistDir.path())) {
          std::string albumName = albumDir.path().filename().string();
          std::regex yearPattern(R"(^\d{4}\s*-\s*(.+)$)");
          std::smatch match;
          if (std::regex_match(albumName, match, yearPattern)) {
            albumName = match[2].str();
          }
          if (albumName == decodedAlbum) {
            albumPath = albumDir.path();
            break;
          }
        }
      }
      if (!albumPath.empty())
        break;
    }
    Json::Value tracksJson(Json::arrayValue);
    if (!albumPath.empty()) {
      std::vector<fs::path> audioFiles;
      for (const auto &entry : fs::directory_iterator(albumPath)) {
        if (entry.is_regular_file()) {
          std::string ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".flac" || ext == ".mp3" || ext == ".m4a") {
            audioFiles.push_back(entry.path());
          }
        }
      }
      std::sort(audioFiles.begin(), audioFiles.end());
      int trackNum = 1;
      for (const auto &trackPath : audioFiles) {
        Json::Value trackObj;
        trackObj["path"] = trackPath.string();
        std::string title = trackPath.stem().string();
        std::regex trackPattern(R"(^\d+\.?\s*(.+)$)");
        std::smatch match;
        if (std::regex_match(title, match, trackPattern)) {
          title = match[1].str();
        }
        trackObj["title"] = title;
        trackObj["artist"] = albumPath.parent_path().filename().string();
        trackObj["album"] = decodedAlbum;
        trackObj["duration"] = 0;
        trackObj["track"] = trackNum++;
        trackObj["year"] = "";
        trackObj["genre"] = "";
        tracksJson.append(trackObj);
      }
    }
    response["status"] = "success";
    response["tracks"] = tracksJson;
    response["count"] = static_cast<int>(tracksJson.size());
  } catch (const std::exception &e) {
    response["status"] = "error";
    response["message"] = e.what();
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(drogon::k200OK);
  callback(resp);
}
