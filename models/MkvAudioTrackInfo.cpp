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
    // Используем оригинальные имена полей FFmpeg (с подчеркиванием)
    track.sampleRate = codec_params->sample_rate; // было sampleRate
    track.channels = codec_params->ch_layout.nb_channels;
    if (codec_params->ch_layout.nb_channels > 0) {
      std::string buf(256, '\0');
      // Правильное имя функции - av_channel_layout_describe
      av_channel_layout_describe(&codec_params->ch_layout, buf.data(),
                                 buf.size());
      buf.resize(std::strlen(buf.c_str()));
      track.channelLayout = std::move(buf);
    }
    track.bitRate = codec_params->bit_rate; // было bitRate
    if (auto *tag =
            av_dict_get(stream->metadata, "default_track", nullptr, 0)) {
      track.isDefault = std::string_view{tag->value} == "1";
    }
    if (auto *tag = av_dict_get(stream->metadata, "forced_track", nullptr, 0)) {
      track.isForced = std::string_view{tag->value} == "1";
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
  std::println("Stream index: {}", track.streamIndex);
  std::println("Codec: {}", track.codecName);
  std::println("Language: {}", track.language);
  std::println("Title: {}", track.title.empty() ? "(none)" : track.title);
  std::println("Sample rate: {} Hz", track.sampleRate);
  std::println("Channels: {}", track.channels);
  std::println("Channel layout: {}", track.channelLayout);
  std::println("Bitrate: {} bps", track.bitRate);
  std::println("Default: {}", track.isDefault ? "Yes" : "No");
  std::println("Forced: {}", track.isForced ? "Yes" : "No");
  std::println();
}

void MkvAudioTrackInfo::printAllTracks() const {
  for (auto i : std::views::iota(0, static_cast<int>(audio_tracks_.size()))) {
    printTrackInfo(i);
  }
}

std::optional<MkvAudioTrackInfo::AudioTrack>
MkvAudioTrackInfo::getTrackByStreamIndex(int streamIndex) const {
  auto it = std::ranges::find_if(audio_tracks_,
                                 [streamIndex](const AudioTrack &track) {
                                   return track.streamIndex == streamIndex;
                                 });
  if (it != audio_tracks_.end()) {
    return *it;
  }
  return std::nullopt;
}
