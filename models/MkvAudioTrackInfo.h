#pragma once

#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

class MKVAudioTrackInfo {
public:
  struct AudioTrack {
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

  MKVAudioTrackInfo();
  ~MKVAudioTrackInfo();

  bool openFile(const std::string &filepath);
  const std::vector<AudioTrack> &getAudioTracks() const;

  int getTrackCount() const;
  void printTrackInfo(int index) const;
  void printAllTracks() const;
  const AudioTrack *getTrackByStreamIndex(int stream_index) const;

private:
  AVFormatContext *format_ctx;
  std::vector<AudioTrack> audio_tracks;
  std::string filename;
  void extractAudioTracks();
};
