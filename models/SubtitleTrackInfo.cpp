#include "SubtitleTrackInfo.h"
#include <algorithm>
#include <format>
#include <ranges>
#include <span>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

std::expected<void, std::string>
SubtitleTrackInfo::openFile(std::string_view filepath) {
  filename_ = filepath;
  AVFormatContext *ctx{};
  if (int ret = avformat_open_input(&ctx, filepath.data(), nullptr, nullptr);
      ret < 0) {
    return std::unexpected(std::format("Failed to open file: {}", filepath));
  }
  format_ctx_.reset(ctx);
  if (int ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
      ret < 0) {
    return std::unexpected("Failed to find stream info");
  }
  extractSubtitleTracks();
  return {};
}

void SubtitleTrackInfo::extractSubtitleTracks() {
  subtitle_tracks_.clear();
  auto streams = std::span{format_ctx_->streams, format_ctx_->nb_streams};
  for (auto &&[idx, stream] : streams | std::views::enumerate) {
    auto *codec_params = stream->codecpar;
    if (codec_params->codec_type != AVMEDIA_TYPE_SUBTITLE) {
      continue;
    }
    SubtitleTrack track;
    track.streamIndex = static_cast<int>(idx);
    if (auto *codec = avcodec_find_decoder(codec_params->codec_id)) {
      track.codecName = codec->name;
    }
    if (auto *tag = av_dict_get(stream->metadata, "language", nullptr, 0)) {
      track.language = tag->value;
    }
    if (auto *tag = av_dict_get(stream->metadata, "title", nullptr, 0)) {
      track.title = tag->value;
    }
    if (auto *tag =
            av_dict_get(stream->metadata, "default_track", nullptr, 0)) {
      track.isDefault = std::string_view{tag->value} == "1";
    }
    if (auto *tag = av_dict_get(stream->metadata, "forced_track", nullptr, 0)) {
      track.isForced = std::string_view{tag->value} == "1";
    }
    subtitle_tracks_.push_back(std::move(track));
  }
}

std::optional<SubtitleTrackInfo::SubtitleTrack>
SubtitleTrackInfo::getTrackByStreamIndex(int streamIndex) const {
  auto it = std::ranges::find_if(subtitle_tracks_,
                                 [streamIndex](const SubtitleTrack &track) {
                                   return track.streamIndex == streamIndex;
                                 });
  if (it != subtitle_tracks_.end()) {
    return *it;
  }
  return std::nullopt;
}
