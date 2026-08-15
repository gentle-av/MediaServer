#include "VideoThumbnailer.h"

#include <iostream>
#include <string>
#include <vector>

void VideoThumbnailer::logFFmpegError(int errorCode,
                                      const std::string &context) {
  char errorBuf[256];
  av_strerror(errorCode, errorBuf, sizeof(errorBuf));
  std::cerr << "FFmpeg error in " << context << ": " << errorBuf << std::endl;
}

bool VideoThumbnailer::extractThumbnail(const std::string &videoPath,
                                        const std::string &outputPath,
                                        double timeInSeconds) {
  AVFormatContext *pFormatContext = nullptr;
  int ret = 0;
  ret =
      avformat_open_input(&pFormatContext, videoPath.c_str(), nullptr, nullptr);
  if (ret < 0) {
    logFFmpegError(ret, "avformat_open_input");
    return false;
  }
  ret = avformat_find_stream_info(pFormatContext, nullptr);
  if (ret < 0) {
    logFFmpegError(ret, "avformat_find_stream_info");
    avformat_close_input(&pFormatContext);
    return false;
  }
  int videoStreamIndex = -1;
  AVCodecParameters *codecParams = nullptr;
  AVStream *videoStream = nullptr;
  for (unsigned int i = 0; i < pFormatContext->nb_streams; i++) {
    if (pFormatContext->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i;
      codecParams = pFormatContext->streams[i]->codecpar;
      videoStream = pFormatContext->streams[i];
      break;
    }
  }
  if (videoStreamIndex == -1) {
    std::cerr << "No video stream found" << std::endl;
    avformat_close_input(&pFormatContext);
    return false;
  }
  std::cout << "Video stream found: index=" << videoStreamIndex
            << ", width=" << codecParams->width
            << ", height=" << codecParams->height << std::endl;
  const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
  if (!codec) {
    std::cerr << "Codec not found" << std::endl;
    avformat_close_input(&pFormatContext);
    return false;
  }
  AVCodecContext *codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
    std::cerr << "Failed to allocate codec context" << std::endl;
    avformat_close_input(&pFormatContext);
    return false;
  }
  ret = avcodec_parameters_to_context(codecContext, codecParams);
  if (ret < 0) {
    logFFmpegError(ret, "avcodec_parameters_to_context");
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  codecContext->time_base = videoStream->time_base;
  ret = avcodec_open2(codecContext, codec, nullptr);
  if (ret < 0) {
    logFFmpegError(ret, "avcodec_open2");
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  double timeBase = av_q2d(videoStream->time_base);
  int64_t target_pts = (int64_t)(timeInSeconds / timeBase);
  std::cout << "Target time: " << timeInSeconds << "s" << std::endl;
  std::cout << "Time base: " << timeBase << std::endl;
  std::cout << "Target PTS: " << target_pts << std::endl;
  ret = av_seek_frame(pFormatContext, videoStreamIndex, target_pts,
                      AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    logFFmpegError(ret, "av_seek_frame");
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  avcodec_flush_buffers(codecContext);
  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  AVFrame *frameRGB = av_frame_alloc();
  if (!packet || !frame || !frameRGB) {
    std::cerr << "Failed to allocate frames/packets" << std::endl;
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  struct SwsContext *sws_ctx = sws_getContext(
      codecContext->width, codecContext->height, codecContext->pix_fmt,
      codecContext->width, codecContext->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
      nullptr, nullptr, nullptr);
  if (!sws_ctx) {
    std::cerr << "Failed to create SwsContext" << std::endl;
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width,
                                          codecContext->height, 1);
  if (numBytes <= 0) {
    std::cerr << "Invalid image size" << std::endl;
    sws_freeContext(sws_ctx);
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  std::vector<uint8_t> buffer(numBytes);
  av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer.data(),
                       AV_PIX_FMT_RGB24, codecContext->width,
                       codecContext->height, 1);
  bool success = false;
  int frameCount = 0;
  const int maxFrames = 1000;
  double lastGoodTime = -1;
  AVFrame *savedFrame = av_frame_alloc();
  if (!savedFrame) {
    std::cerr << "Failed to allocate saved frame" << std::endl;
    sws_freeContext(sws_ctx);
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_free_context(&codecContext);
    avformat_close_input(&pFormatContext);
    return false;
  }
  savedFrame->format = AV_PIX_FMT_RGB24;
  savedFrame->width = codecContext->width;
  savedFrame->height = codecContext->height;
  av_frame_get_buffer(savedFrame, 1);
  while (av_read_frame(pFormatContext, packet) >= 0 && frameCount < maxFrames) {
    if (packet->stream_index == videoStreamIndex) {
      ret = avcodec_send_packet(codecContext, packet);
      if (ret == 0) {
        while (ret >= 0) {
          ret = avcodec_receive_frame(codecContext, frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
          } else if (ret < 0) {
            break;
          }
          int64_t framePts = frame->pts;
          if (framePts == AV_NOPTS_VALUE) {
            framePts = frame->best_effort_timestamp;
          }
          double frameTime = framePts * timeBase;
          frameCount++;
          if (frameTime <= timeInSeconds) {
            lastGoodTime = frameTime;
            sws_scale(sws_ctx, frame->data, frame->linesize, 0,
                      codecContext->height, savedFrame->data,
                      savedFrame->linesize);
            std::cout << "Frame " << frameCount << ": PTS=" << framePts
                      << ", time=" << frameTime << "s (keeping)" << std::endl;
          } else {
            std::cout << "Frame " << frameCount << ": PTS=" << framePts
                      << ", time=" << frameTime << "s (stopping)" << std::endl;
            if (lastGoodTime >= 0) {
              std::cout << "Using last frame at " << lastGoodTime << "s"
                        << std::endl;
              FILE *file = fopen(outputPath.c_str(), "wb");
              if (file) {
                fprintf(file, "P6\n%d %d\n255\n", codecContext->width,
                        codecContext->height);
                fwrite(savedFrame->data[0], 1,
                       savedFrame->linesize[0] * codecContext->height, file);
                fclose(file);
                success = true;
                std::cout << "Frame saved to: " << outputPath << std::endl;
              }
            }
            break;
          }
        }
      }
    }
    av_packet_unref(packet);
    if (success || frameCount >= maxFrames)
      break;
  }
  if (!success && lastGoodTime >= 0) {
    std::cout << "Using last frame at " << lastGoodTime << "s (end of file)"
              << std::endl;
    FILE *file = fopen(outputPath.c_str(), "wb");
    if (file) {
      fprintf(file, "P6\n%d %d\n255\n", codecContext->width,
              codecContext->height);
      fwrite(savedFrame->data[0], 1,
             savedFrame->linesize[0] * codecContext->height, file);
      fclose(file);
      success = true;
      std::cout << "Frame saved to: " << outputPath << std::endl;
    }
  }
  if (!success) {
    std::cerr << "No suitable frame found" << std::endl;
  }
  sws_freeContext(sws_ctx);
  av_packet_free(&packet);
  av_frame_free(&frame);
  av_frame_free(&frameRGB);
  av_frame_free(&savedFrame);
  avcodec_free_context(&codecContext);
  avformat_close_input(&pFormatContext);
  return success;
}
