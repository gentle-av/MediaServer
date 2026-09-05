#include "NewVideoThumbnailExtractor.h"
#include "VideoIntegrityChecker.h"
#include "VideoThumbnailer.h"

NewVideoThumbnailExtractor::NewVideoThumbnailExtractor(
    const std::string &watchDirectory, ImageDatabase &database)
    : watchDirectory(watchDirectory), database(database), running(false) {}

NewVideoThumbnailExtractor::~NewVideoThumbnailExtractor() { stop(); }

void NewVideoThumbnailExtractor::start() {
  if (running) {
    return;
  }
  running = true;
  watcher = std::make_unique<DirectoryWatcher>(
      watchDirectory,
      [this](const std::filesystem::path &path) { onVideoAdded(path); });
}

void NewVideoThumbnailExtractor::stop() {
  if (watcher) {
    watcher->stop();
    watcher.reset();
  }
  running = false;
}

bool NewVideoThumbnailExtractor::isRunning() const { return running; }

void NewVideoThumbnailExtractor::onVideoAdded(
    const std::filesystem::path &videoPath) {
  if (!running) {
    return;
  }
  std::string pathStr = videoPath.string();
  if (database.imageExists(pathStr)) {
    return;
  }
  VideoIntegrityChecker::Status status = VideoIntegrityChecker::check(pathStr);
  if (status != VideoIntegrityChecker::Status::Valid) {
    return;
  }
  processVideo(videoPath);
}

void NewVideoThumbnailExtractor::processVideo(
    const std::filesystem::path &videoPath) {
  VideoThumbnailer thumbnailer;
  auto frame = thumbnailer.extractFrame(videoPath, 300.0);
  if (!frame.has_value()) {
    return;
  }
  std::vector<unsigned char> buffer = std::move(frame.value());
  if (buffer.empty()) {
    return;
  }
  ImageData imageData;
  imageData.data = std::move(buffer);
  imageData.mimeType = "image/jpeg";
  database.saveImage(videoPath.string(), imageData);
}
