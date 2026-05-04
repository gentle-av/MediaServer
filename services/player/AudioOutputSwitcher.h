#pragma once

#include <string>
#include <vector>

class AudioOutputSwitcher {
public:
  bool switchToSpeakers();
  bool switchToHeadphones();
  std::string getCurrentOutput() const;
  std::vector<std::string> getAvailableOutputs() const;
};
