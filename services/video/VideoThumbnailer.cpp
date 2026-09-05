#include "VideoThumbnailer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

VideoThumbnailer::VideoThumbnailer() = default;

VideoThumbnailer::~VideoThumbnailer() { cleanup(); }

void VideoThumbnailer::cleanup() const {
  if (swsContext) {
    sws_freeContext(swsContext);
    swsContext = nullptr;
  }
  if (rgbFrame) {
    av_frame_free(&rgbFrame);
    rgbFrame = nullptr;
  }
  if (frame) {
    av_frame_free(&frame);
    frame = nullptr;
  }
  if (packet) {
    av_packet_free(&packet);
    packet = nullptr;
  }
  if (codecContext) {
    avcodec_free_context(&codecContext);
    codecContext = nullptr;
  }
  if (formatContext) {
    avformat_close_input(&formatContext);
    formatContext = nullptr;
  }
  videoStreamIndex = -1;
}

bool VideoThumbnailer::openVideo(const std::string &filename) const {
  cleanup();
  if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) <
      0) {
    return false;
  }
  if (avformat_find_stream_info(formatContext, nullptr) < 0) {
    cleanup();
    return false;
  }
  videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1,
                                         -1, nullptr, 0);
  if (videoStreamIndex < 0) {
    cleanup();
    return false;
  }
  const AVCodec *codec = avcodec_find_decoder(
      formatContext->streams[videoStreamIndex]->codecpar->codec_id);
  if (!codec) {
    cleanup();
    return false;
  }
  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
    cleanup();
    return false;
  }
  if (avcodec_parameters_to_context(
          codecContext, formatContext->streams[videoStreamIndex]->codecpar) <
      0) {
    cleanup();
    return false;
  }
  if (avcodec_open2(codecContext, codec, nullptr) < 0) {
    cleanup();
    return false;
  }
  width = codecContext->width;
  height = codecContext->height;
  frame = av_frame_alloc();
  rgbFrame = av_frame_alloc();
  packet = av_packet_alloc();
  if (!frame || !rgbFrame || !packet) {
    cleanup();
    return false;
  }
  return true;
}

bool VideoThumbnailer::seekToTimestamp(double timestampSeconds) const {
  if (!formatContext || videoStreamIndex < 0) {
    return false;
  }
  AVStream *stream = formatContext->streams[videoStreamIndex];
  int64_t timestamp =
      static_cast<int64_t>(timestampSeconds / av_q2d(stream->time_base));
  if (av_seek_frame(formatContext, videoStreamIndex, timestamp,
                    AVSEEK_FLAG_BACKWARD) < 0) {
    return false;
  }
  avcodec_flush_buffers(codecContext);
  return true;
}

bool VideoThumbnailer::decodeFrame() const {
  while (true) {
    int ret = av_read_frame(formatContext, packet);
    if (ret < 0) {
      return false;
    }
    if (packet->stream_index == videoStreamIndex) {
      ret = avcodec_send_packet(codecContext, packet);
      if (ret < 0) {
        av_packet_unref(packet);
        continue;
      }
      ret = avcodec_receive_frame(codecContext, frame);
      if (ret == 0) {
        av_packet_unref(packet);
        return true;
      }
    }
    av_packet_unref(packet);
  }
}

std::vector<uint8_t> VideoThumbnailer::convertToRGB() const {
  if (!frame || width <= 0 || height <= 0) {
    return {};
  }
  swsContext =
      sws_getContext(width, height, codecContext->pix_fmt, width, height,
                     AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!swsContext) {
    return {};
  }
  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
  std::vector<uint8_t> rgbData(numBytes);
  av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbData.data(),
                       AV_PIX_FMT_RGB24, width, height, 1);
  sws_scale(swsContext, frame->data, frame->linesize, 0, height, rgbFrame->data,
            rgbFrame->linesize);
  return rgbData;
}

std::optional<std::vector<uint8_t>>
VideoThumbnailer::extractFrame(const std::filesystem::path &videoPath,
                               double timestampSeconds) const {
  if (!openVideo(videoPath.string())) {
    return std::nullopt;
  }
  if (!seekToTimestamp(timestampSeconds)) {
    cleanup();
    return std::nullopt;
  }
  if (!decodeFrame()) {
    cleanup();
    return std::nullopt;
  }
  auto rgbData = convertToRGB();
  cleanup();
  if (rgbData.empty()) {
    return std::nullopt;
  }
  return rgbData;
}
