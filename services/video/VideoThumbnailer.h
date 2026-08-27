#pragma once

#include <filesystem>
#include <opencv5/opencv2/cudacodec.hpp>
#include <opencv5/opencv2/opencv.hpp>
#include <optional>

class VideoThumbnailer {
public:
  [[nodiscard]] std::optional<cv::Mat>
  extractFrame(const std::filesystem::path &videoPath,
               double timestampSeconds) const;
};
