#pragma once

#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

struct AudioChannelInfo {
  int stream_index;
  std::string codec_name;
  std::string language;
  std::string title;
  int sample_rate;
  int channels;
  std::string channel_layout;
  int64_t bit_rate;
  bool is_default;
  bool is_forced;
};

class MkvAudioChannel {
public:
  MkvAudioChannel() = default;
  ~MkvAudioChannel() = default;

  [[nodiscard]] std::vector<AudioChannelInfo>
  getChannels(const std::string &filepath);
  [[nodiscard]] std::optional<AudioChannelInfo>
  getChannelByIndex(const std::string &filepath, int index);
  [[nodiscard]] std::optional<AudioChannelInfo>
  getChannelByStreamIndex(const std::string &filepath, int stream_index);

  [[nodiscard]] bool selectChannel(const std::string &filepath,
                                   int stream_index);
  [[nodiscard]] int getCurrentChannel() const { return current_stream_index_; }
  [[nodiscard]] std::string getCurrentChannelInfo() const;

private:
  std::string current_filepath_;
  int current_stream_index_{-1};
  std::vector<AudioChannelInfo> current_channels_;

  void updateChannels(const std::string &filepath);
};
