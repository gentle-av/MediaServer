#include "services/player/AudioOutputSwitcher.h"
#include "services/AlsaMixer.h"

bool AudioOutputSwitcher::switchToSpeakers() {
  return AlsaMixer::getInstance().switchToSpeakers();
}

bool AudioOutputSwitcher::switchToHeadphones() {
  return AlsaMixer::getInstance().switchToHeadphones();
}

std::string AudioOutputSwitcher::getCurrentOutput() const {
  std::string output = AlsaMixer::getInstance().getCurrentOutput();
  return output.empty() ? "speakers" : output;
}

std::vector<std::string> AudioOutputSwitcher::getAvailableOutputs() const {
  return AlsaMixer::getInstance().getAvailableOutputs();
}
