#pragma once

#include <mutex>
#include <string>
#include <vector>

class AudioOutputSwitcher {
public:
  AudioOutputSwitcher();
  bool switchToSpeakers();
  bool switchToHeadphones();
  std::string getCurrentOutput();
  std::vector<std::string> getAvailableOutputs() const;

private:
  std::string currentOutput;
  mutable std::mutex mutex;
  void detectCurrentOutput();
};
