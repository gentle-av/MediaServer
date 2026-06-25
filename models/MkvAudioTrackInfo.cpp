#include "MkvAudioTrackInfo.h"
#include <cstring>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

MKVAudioTrackInfo::MKVAudioTrackInfo() : format_ctx(nullptr) {
  avformat_network_init();
}

MKVAudioTrackInfo::~MKVAudioTrackInfo() {
  if (format_ctx) {
    avformat_close_input(&format_ctx);
  }
}

bool MKVAudioTrackInfo::openFile(const std::string &filepath) {
  filename = filepath;
  int ret =
      avformat_open_input(&format_ctx, filepath.c_str(), nullptr, nullptr);
  if (ret < 0) {
    std::cerr << "Не удалось открыть файл: " << filepath << std::endl;
    return false;
  }
  ret = avformat_find_stream_info(format_ctx, nullptr);
  if (ret < 0) {
    std::cerr << "Не удалось найти информацию о потоках" << std::endl;
    return false;
  }
  extractAudioTracks();
  return true;
}

void MKVAudioTrackInfo::extractAudioTracks() {
  audio_tracks.clear();
  for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
    AVStream *stream = format_ctx->streams[i];
    AVCodecParameters *codec_params = stream->codecpar;
    if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
      AudioTrack track;
      track.stream_index = i;
      const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
      track.codec_name = codec ? codec->name : "unknown";
      AVDictionaryEntry *lang_tag =
          av_dict_get(stream->metadata, "language", nullptr, 0);
      track.language = lang_tag ? lang_tag->value : "und";
      AVDictionaryEntry *title_tag =
          av_dict_get(stream->metadata, "title", nullptr, 0);
      track.title = title_tag ? title_tag->value : "";
      track.sample_rate = codec_params->sample_rate;
      track.channels = codec_params->ch_layout.nb_channels;
      if (codec_params->ch_layout.nb_channels > 0) {
        char buf[256];
        av_channel_layout_describe(&codec_params->ch_layout, buf, sizeof(buf));
        track.channel_layout = buf;
      } else {
        track.channel_layout = "unknown";
      }
      track.bit_rate = codec_params->bit_rate;
      track.is_default = false;
      track.is_forced = false;
      AVDictionaryEntry *default_tag =
          av_dict_get(stream->metadata, "default_track", nullptr, 0);
      if (default_tag && std::strcmp(default_tag->value, "1") == 0) {
        track.is_default = true;
      }
      AVDictionaryEntry *forced_tag =
          av_dict_get(stream->metadata, "forced_track", nullptr, 0);
      if (forced_tag && std::strcmp(forced_tag->value, "1") == 0) {
        track.is_forced = true;
      }
      audio_tracks.push_back(track);
    }
  }
}

const std::vector<MKVAudioTrackInfo::AudioTrack> &
MKVAudioTrackInfo::getAudioTracks() const {
  return audio_tracks;
}

int MKVAudioTrackInfo::getTrackCount() const { return audio_tracks.size(); }

void MKVAudioTrackInfo::printTrackInfo(int index) const {
  if (index < 0 || index >= static_cast<int>(audio_tracks.size())) {
    std::cout << "Неверный индекс дорожки" << std::endl;
    return;
  }
  const AudioTrack &track = audio_tracks[index];
  std::cout << "=== Аудиодорожка " << index << " ===" << std::endl;
  std::cout << "Индекс потока: " << track.stream_index << std::endl;
  std::cout << "Кодек: " << track.codec_name << std::endl;
  std::cout << "Язык: " << track.language << std::endl;
  std::cout << "Название: " << (track.title.empty() ? "(нет)" : track.title)
            << std::endl;
  std::cout << "Частота: " << track.sample_rate << " Гц" << std::endl;
  std::cout << "Каналы: " << track.channels << std::endl;
  std::cout << "Раскладка: " << track.channel_layout << std::endl;
  std::cout << "Битрейт: " << track.bit_rate << " bps" << std::endl;
  std::cout << "По умолчанию: " << (track.is_default ? "Да" : "Нет")
            << std::endl;
  std::cout << "Принудительная: " << (track.is_forced ? "Да" : "Нет")
            << std::endl;
  std::cout << std::endl;
}

void MKVAudioTrackInfo::printAllTracks() const {
  for (std::size_t i = 0; i < audio_tracks.size(); i++) {
    printTrackInfo(i);
  }
}

const MKVAudioTrackInfo::AudioTrack *
MKVAudioTrackInfo::getTrackByStreamIndex(int stream_index) const {
  for (const auto &track : audio_tracks) {
    if (track.stream_index == stream_index) {
      return &track;
    }
  }
  return nullptr;
}
