#include "VideoThumbnailer.h"

std::optional<cv::Mat>
VideoThumbnailer::extractFrame(const std::filesystem::path &videoPath,
                               double timestampSeconds) const {
  cv::VideoCapture cap(videoPath.string());
  if (!cap.isOpened()) {
    return std::nullopt;
  }
  double fps = cap.get(cv::CAP_PROP_FPS);
  if (fps <= 0) {
    return std::nullopt;
  }
  int targetFrame = static_cast<int>(timestampSeconds * fps);
  cap.set(cv::CAP_PROP_POS_FRAMES, targetFrame);
  cv::Mat frame;
  if (!cap.read(frame)) {
    return std::nullopt;
  }
  return frame;
}
