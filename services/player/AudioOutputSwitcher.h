#pragma once

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
  void detectCurrentOutput();
};
