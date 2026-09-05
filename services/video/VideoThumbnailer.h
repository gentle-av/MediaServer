#pragma once

#include <filesystem>
#include <optional>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

class VideoThumbnailer {
public:
  VideoThumbnailer();
  ~VideoThumbnailer();

  std::optional<std::vector<uint8_t>>
  extractFrame(const std::filesystem::path &videoPath,
               double timestampSeconds) const;

private:
  mutable AVFormatContext *formatContext = nullptr;
  mutable AVCodecContext *codecContext = nullptr;
  mutable AVFrame *frame = nullptr;
  mutable AVFrame *rgbFrame = nullptr;
  mutable AVPacket *packet = nullptr;
  mutable SwsContext *swsContext = nullptr;
  mutable int videoStreamIndex = -1;
  mutable int width = 0;
  mutable int height = 0;

  bool openVideo(const std::string &filename) const;
  bool seekToTimestamp(double timestampSeconds) const;
  bool decodeFrame() const;
  std::vector<uint8_t> convertToRGB() const;
  void cleanup() const;
};
