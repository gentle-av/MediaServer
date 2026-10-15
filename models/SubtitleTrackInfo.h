#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

class SubtitleTrackInfo {
public:
  struct SubtitleTrack {
    int streamIndex{};
    std::string codecName;
    std::string language = "und";
    std::string title;
    bool isDefault{};
    bool isForced{};
  };

  SubtitleTrackInfo() = default;
  ~SubtitleTrackInfo() = default;

  SubtitleTrackInfo(const SubtitleTrackInfo &) = delete;
  SubtitleTrackInfo &operator=(const SubtitleTrackInfo &) = delete;
  SubtitleTrackInfo(SubtitleTrackInfo &&) = default;
  SubtitleTrackInfo &operator=(SubtitleTrackInfo &&) = default;

  [[nodiscard]] std::expected<void, std::string>
  openFile(std::string_view filepath);

  [[nodiscard]] const std::vector<SubtitleTrack> &
  getSubtitleTracks() const noexcept {
    return subtitle_tracks_;
  }

  [[nodiscard]] int getTrackCount() const noexcept {
    return static_cast<int>(subtitle_tracks_.size());
  }

  [[nodiscard]] std::optional<SubtitleTrack>
  getTrackByStreamIndex(int streamIndex) const;

private:
  struct AvFormatDeleter {
    void operator()(AVFormatContext *ctx) const noexcept {
      if (ctx)
        avformat_close_input(&ctx);
    }
  };

  using AvFormatPtr = std::unique_ptr<AVFormatContext, AvFormatDeleter>;

  AvFormatPtr format_ctx_;
  std::vector<SubtitleTrack> subtitle_tracks_;
  std::string filename_;

  void extractSubtitleTracks();
};
