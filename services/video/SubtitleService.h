#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

class SubtitleService {
public:
  struct SubtitleTrack {
    int streamIndex{};
    std::string codecName;
    std::string language;
    std::string title;
    bool isDefault{};
    bool isForced{};
  };

  SubtitleService() = default;
  ~SubtitleService() = default;

  SubtitleService(const SubtitleService &) = delete;
  SubtitleService &operator=(const SubtitleService &) = delete;

  [[nodiscard]] std::expected<std::vector<SubtitleTrack>, std::string>
  getSubtitleTracks();

  [[nodiscard]] std::expected<bool, std::string>
  setSubtitleTrack(int streamIndex);

  [[nodiscard]] std::expected<std::optional<SubtitleTrack>, std::string>
  getCurrentSubtitleTrack();

  [[nodiscard]] std::expected<bool, std::string> disableSubtitles();
};
