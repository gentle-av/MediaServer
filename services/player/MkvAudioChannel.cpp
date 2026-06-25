#include "MkvAudioChannel.h"
#include "models/MkvAudioTrackInfo.h"
#include <format>

std::vector<AudioChannelInfo>
MkvAudioChannel::getChannels(const std::string &filepath) {
  updateChannels(filepath);
  return current_channels_;
}

std::optional<AudioChannelInfo>
MkvAudioChannel::getChannelByIndex(const std::string &filepath, int index) {
  updateChannels(filepath);
  if (index < 0 || index >= static_cast<int>(current_channels_.size())) {
    return std::nullopt;
  }
  return current_channels_[index];
}

std::optional<AudioChannelInfo>
MkvAudioChannel::getChannelByStreamIndex(const std::string &filepath,
                                         int stream_index) {
  updateChannels(filepath);
  for (const auto &channel : current_channels_) {
    if (channel.stream_index == stream_index) {
      return channel;
    }
  }
  return std::nullopt;
}

bool MkvAudioChannel::selectChannel(const std::string &filepath,
                                    int stream_index) {
  auto channel = getChannelByStreamIndex(filepath, stream_index);
  if (!channel.has_value()) {
    return false;
  }
  current_filepath_ = filepath;
  current_stream_index_ = stream_index;
  return true;
}

std::string MkvAudioChannel::getCurrentChannelInfo() const {
  if (current_stream_index_ < 0) {
    return "default";
  }
  return std::format("aid: {}", current_stream_index_);
}

void MkvAudioChannel::updateChannels(const std::string &filepath) {
  if (filepath != current_filepath_) {
    MkvAudioTrackInfo reader;
    auto result = reader.openFile(filepath);
    if (result.has_value()) {
      current_channels_.clear();
      for (const auto &track : reader.getAudioTracks()) {
        AudioChannelInfo info{.stream_index = track.stream_index,
                              .codec_name = track.codec_name,
                              .language = track.language,
                              .title = track.title,
                              .sample_rate = track.sample_rate,
                              .channels = track.channels,
                              .channel_layout = track.channel_layout,
                              .bit_rate = track.bit_rate,
                              .is_default = track.is_default,
                              .is_forced = track.is_forced};
        current_channels_.push_back(info);
      }
      current_filepath_ = filepath;
    }
  }
}
