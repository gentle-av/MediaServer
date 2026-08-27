#include "ManualThumbnailUpdater.h"
#include "VideoIntegrityChecker.h"
#include "VideoThumbnailer.h"

ManualThumbnailUpdater::ManualThumbnailUpdater(ImageDatabase &database)
    : database(database) {}

bool ManualThumbnailUpdater::updateThumbnail(
    const std::filesystem::path &videoPath) {
  return updateThumbnail(videoPath, 300.0);
}

bool ManualThumbnailUpdater::updateThumbnail(
    const std::filesystem::path &videoPath, double timestampSeconds) {
  if (!std::filesystem::exists(videoPath)) {
    return false;
  }
  std::string pathStr = videoPath.string();
  VideoIntegrityChecker::Status status = VideoIntegrityChecker::check(pathStr);
  if (status != VideoIntegrityChecker::Status::Valid) {
    return false;
  }
  return processVideo(videoPath, timestampSeconds);
}

bool ManualThumbnailUpdater::processVideo(
    const std::filesystem::path &videoPath, double timestampSeconds) {
  VideoThumbnailer thumbnailer;
  auto frame = thumbnailer.extractFrame(videoPath, timestampSeconds);
  if (!frame.has_value()) {
    return false;
  }
  cv::Mat cpuFrame = frame.value();
  std::vector<unsigned char> buffer;
  std::vector<int> compressionParams = {cv::IMWRITE_JPEG_QUALITY, 90};
  bool success = cv::imencode(".jpg", cpuFrame, buffer, compressionParams);
  if (!success || buffer.empty()) {
    return false;
  }
  ImageData imageData;
  imageData.data = std::move(buffer);
  imageData.mimeType = "image/jpeg";
  return database.saveImage(videoPath.string(), imageData);
}
