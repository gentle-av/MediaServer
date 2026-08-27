#include "VideoThumbnailer.h"

[[nodiscard]] std::optional<cv::Mat>
VideoThumbnailer::extractFrame(const std::filesystem::path &videoPath,
                               double timestampSeconds) const {
  cv::VideoCapture info(videoPath.string());
  if (!info.isOpened()) {
    return std::nullopt;
  }
  double fps = info.get(cv::CAP_PROP_FPS);
  info.release();
  if (fps <= 0) {
    return std::nullopt;
  }
  auto reader = cv::cudacodec::createVideoReader(videoPath.string());
  if (!reader) {
    return std::nullopt;
  }
  int targetFrame = static_cast<int>(timestampSeconds * fps);
  cv::cuda::GpuMat gpuFrame;
  for (int i = 0; i <= targetFrame; ++i) {
    if (!reader->nextFrame(gpuFrame)) {
      return std::nullopt;
    }
  }
  cv::Mat cpuFrame;
  gpuFrame.download(cpuFrame);
  return cpuFrame;
}
