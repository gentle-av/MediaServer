#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

class VideoThumbnailer {
private:
  void logFFmpegError(int errorCode, const std::string &context);

public:
  bool extractThumbnail(const std::string &videoPath,
                        const std::string &outputPath, double timeInSeconds);
};
