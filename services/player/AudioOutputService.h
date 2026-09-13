#pragma once

#include "AudioOutputSwitcher.h"
#include <memory>
#include <string>
#include <vector>

class AudioOutputService {
public:
  AudioOutputService();
  ~AudioOutputService() = default;

  int getVolume() const;
  void setVolume(int percent);
  void adjustVolume(int delta);
  void toggleMute();
  bool isMuted() const;
  bool switchToSpeakers();
  bool switchToHeadphones();
  std::string getCurrentOutput() const;
  std::vector<std::string> getAvailableOutputs() const;

private:
  std::unique_ptr<AudioOutputSwitcher> outputSwitcher;
};
