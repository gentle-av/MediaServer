#include "services/video/ThumbnailService.h"
#include "services/video/FileSystemService.h"
#include "services/video/ThumbnailCache.h"
#include <cstdio>
#include <filesystem>
#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static bool executeCommandGetOutput(const std::vector<std::string> &args,
                                    std::string &output) {
  if (args.empty())
    return false;
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[1]);
  char buffer[128];
  ssize_t n;
  while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    output += buffer;
  }
  close(pipefd[0]);
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

ThumbnailService &ThumbnailService::getInstance() {
  static ThumbnailService instance;
  return instance;
}

bool ThumbnailService::isVideoValid(const std::string &videoPath) {
  std::vector<std::string> args = {"ffprobe",         "-v",     "error",
                                   "-select_streams", "v:0",    "-show_streams",
                                   "-show_format",    videoPath};
  std::string result;
  executeCommandGetOutput(args, result);
  return !result.empty();
}

void ThumbnailService::initCache(const std::string &dbPath) {
  if (!isCacheInitialized) {
    std::string path = dbPath;
    if (path.empty()) {
      const char *home = getenv("HOME");
      path = home ? std::string(home) + "/.local/share/media-explorer/video.db"
                  : "./video.db";
      fs::create_directories(fs::path(path).parent_path());
    }
    ThumbnailCache::getInstance().init(path);
    isCacheInitialized = true;
  }
}

bool ThumbnailService::generateRawThumbnail(const std::string &videoPath,
                                            int width, int quality,
                                            std::vector<uint8_t> &imageData) {
  if (!isVideoValid(videoPath)) {
    return false;
  }
  try {
    ffmpegthumbnailer::VideoThumbnailer thumbnailer;
    thumbnailer.setThumbnailSize(width);
    thumbnailer.setSeekPercentage(50);
    thumbnailer.setImageQuality(quality);
    thumbnailer.setMaintainAspectRatio(true);
    thumbnailer.generateThumbnail(videoPath, Jpeg, imageData);
    return !imageData.empty();
  } catch (const std::exception &) {
    return false;
  }
}

std::string
ThumbnailService::generateThumbnailBase64(const std::string &videoPath,
                                          int width, int quality) {
  initCache();
  return ThumbnailCache::getInstance().getThumbnail(videoPath, width, quality);
}

void ThumbnailService::clearCache() {
  if (isCacheInitialized) {
    ThumbnailCache::getInstance().clearCache();
  }
}

void ThumbnailService::shutdownCache() {
  if (isCacheInitialized) {
    ThumbnailCache::getInstance().shutdown();
    isCacheInitialized = false;
  }
}

std::string ThumbnailService::base64Encode(const std::vector<uint8_t> &data) {
  static const char *base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string base64;
  base64.reserve(((data.size() + 2) / 3) * 4);
  for (size_t i = 0; i < data.size(); i += 3) {
    uint32_t block = 0;
    int padding = 0;
    block |= (uint32_t)data[i] << 16;
    if (i + 1 < data.size()) {
      block |= (uint32_t)data[i + 1] << 8;
    } else {
      padding++;
    }
    if (i + 2 < data.size()) {
      block |= (uint32_t)data[i + 2];
    } else {
      padding++;
    }
    base64.push_back(base64_chars[(block >> 18) & 0x3F]);
    base64.push_back(base64_chars[(block >> 12) & 0x3F]);
    base64.push_back(padding >= 2 ? '=' : base64_chars[(block >> 6) & 0x3F]);
    base64.push_back(padding >= 1 ? '=' : base64_chars[block & 0x3F]);
  }
  return base64;
}

Json::Value
ThumbnailService::generateThumbnailResponse(const std::string &videoPath,
                                            int width, int quality) {
  auto &fsService = FileSystemService::getInstance();
  Json::Value response;
  if (!fsService.isPathAllowed(videoPath)) {
    response["success"] = false;
    response["error"] = "Access denied";
    return response;
  }
  if (!isVideoValid(videoPath)) {
    response["success"] = false;
    response["error"] = "Video file is corrupted or invalid";
    response["use_icon"] = true;
    return response;
  }
  std::string base64Thumbnail =
      generateThumbnailBase64(videoPath, width, quality);
  if (base64Thumbnail.empty()) {
    response["success"] = false;
    response["error"] = "Could not generate thumbnail";
    response["use_icon"] = true;
    return response;
  }
  response["success"] = true;
  response["thumbnail"] = "data:image/jpeg;base64," + base64Thumbnail;
  return response;
}
