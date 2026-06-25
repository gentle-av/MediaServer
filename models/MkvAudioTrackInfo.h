#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

class MkvAudioTrackInfo {
public:
  struct AudioTrack {
    int stream_index{};
    std::string codec_name;
    std::string language = "und";
    std::string title;
    int sample_rate{};
    int channels{};
    std::string channel_layout;
    int64_t bit_rate{};
    bool is_default{};
    bool is_forced{};
  };

  MkvAudioTrackInfo() = default;
  ~MkvAudioTrackInfo() = default;

  MkvAudioTrackInfo(const MkvAudioTrackInfo &) = delete;
  MkvAudioTrackInfo &operator=(const MkvAudioTrackInfo &) = delete;
  MkvAudioTrackInfo(MkvAudioTrackInfo &&) = default;
  MkvAudioTrackInfo &operator=(MkvAudioTrackInfo &&) = default;

  [[nodiscard]] std::expected<void, std::string>
  openFile(std::string_view filepath);

  [[nodiscard]] const std::vector<AudioTrack> &getAudioTracks() const noexcept {
    return audio_tracks_;
  }

  [[nodiscard]] int getTrackCount() const noexcept {
    return static_cast<int>(audio_tracks_.size());
  }

  void printTrackInfo(int index) const;
  void printAllTracks() const;

  [[nodiscard]] std::optional<AudioTrack>
  getTrackByStreamIndex(int stream_index) const;

private:
  struct AvFormatDeleter {
    void operator()(AVFormatContext *ctx) const noexcept {
      if (ctx)
        avformat_close_input(&ctx);
    }
  };

  using AvFormatPtr = std::unique_ptr<AVFormatContext, AvFormatDeleter>;

  AvFormatPtr format_ctx_;
  std::vector<AudioTrack> audio_tracks_;
  std::string filename_;

  void extractAudioTracks();
};
