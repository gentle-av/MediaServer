#include "MkvAudioTrackInfo.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <print>
#include <ranges>
#include <span>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

std::expected<void, std::string>
MkvAudioTrackInfo::openFile(std::string_view filepath) {
  avformat_network_init();
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
  extractAudioTracks();
  return {};
}

void MkvAudioTrackInfo::extractAudioTracks() {
  audio_tracks_.clear();
  auto streams = std::span{format_ctx_->streams, format_ctx_->nb_streams};
  for (auto &&[idx, stream] : streams | std::views::enumerate) {
    auto *codec_params = stream->codecpar;

    if (codec_params->codec_type != AVMEDIA_TYPE_AUDIO) {
      continue;
    }
    AudioTrack track;
    track.stream_index = static_cast<int>(idx);
    if (auto *codec = avcodec_find_decoder(codec_params->codec_id)) {
      track.codec_name = codec->name;
    }
    if (auto *tag = av_dict_get(stream->metadata, "language", nullptr, 0)) {
      track.language = tag->value;
    }
    if (auto *tag = av_dict_get(stream->metadata, "title", nullptr, 0)) {
      track.title = tag->value;
    }
    track.sample_rate = codec_params->sample_rate;
    track.channels = codec_params->ch_layout.nb_channels;
    if (codec_params->ch_layout.nb_channels > 0) {
      std::string buf(256, '\0');
      av_channel_layout_describe(&codec_params->ch_layout, buf.data(),
                                 buf.size());
      buf.resize(std::strlen(buf.c_str()));
      track.channel_layout = std::move(buf);
    }
    track.bit_rate = codec_params->bit_rate;
    if (auto *tag =
            av_dict_get(stream->metadata, "default_track", nullptr, 0)) {
      track.is_default = std::string_view{tag->value} == "1";
    }
    if (auto *tag = av_dict_get(stream->metadata, "forced_track", nullptr, 0)) {
      track.is_forced = std::string_view{tag->value} == "1";
    }
    audio_tracks_.push_back(std::move(track));
  }
}

void MkvAudioTrackInfo::printTrackInfo(int index) const {
  if (index < 0 || index >= static_cast<int>(audio_tracks_.size())) {
    std::println("Invalid track index");
    return;
  }

  const auto &track = audio_tracks_[index];
  std::println("=== Audio Track {} ===", index);
  std::println("Stream index: {}", track.stream_index);
  std::println("Codec: {}", track.codec_name);
  std::println("Language: {}", track.language);
  std::println("Title: {}", track.title.empty() ? "(none)" : track.title);
  std::println("Sample rate: {} Hz", track.sample_rate);
  std::println("Channels: {}", track.channels);
  std::println("Channel layout: {}", track.channel_layout);
  std::println("Bitrate: {} bps", track.bit_rate);
  std::println("Default: {}", track.is_default ? "Yes" : "No");
  std::println("Forced: {}", track.is_forced ? "Yes" : "No");
  std::println();
}

void MkvAudioTrackInfo::printAllTracks() const {
  for (auto i : std::views::iota(0, static_cast<int>(audio_tracks_.size()))) {
    printTrackInfo(i);
  }
}

std::optional<MkvAudioTrackInfo::AudioTrack>
MkvAudioTrackInfo::getTrackByStreamIndex(int stream_index) const {
  auto it = std::ranges::find_if(audio_tracks_,
                                 [stream_index](const AudioTrack &track) {
                                   return track.stream_index == stream_index;
                                 });
  if (it != audio_tracks_.end()) {
    return *it;
  }
  return std::nullopt;
}
