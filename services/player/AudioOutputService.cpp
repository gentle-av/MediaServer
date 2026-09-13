#include "AudioOutputService.h"
#include "../../services/system/AlsaMixer.h"

AudioOutputService::AudioOutputService() {
  outputSwitcher = std::make_unique<AudioOutputSwitcher>();
}

int AudioOutputService::getVolume() const {
  return AlsaMixer::getInstance().getVolume();
}

void AudioOutputService::setVolume(int percent) {
  AlsaMixer::getInstance().setVolume(percent);
}

void AudioOutputService::adjustVolume(int delta) {
  if (delta > 0) {
    AlsaMixer::getInstance().increaseVolume(delta);
  } else {
    AlsaMixer::getInstance().decreaseVolume(-delta);
  }
}

void AudioOutputService::toggleMute() { AlsaMixer::getInstance().toggleMute(); }

bool AudioOutputService::isMuted() const {
  return AlsaMixer::getInstance().isMuted();
}

bool AudioOutputService::switchToSpeakers() {
  return outputSwitcher->switchToSpeakers();
}

bool AudioOutputService::switchToHeadphones() {
  return outputSwitcher->switchToHeadphones();
}

std::string AudioOutputService::getCurrentOutput() const {
  return outputSwitcher->getCurrentOutput();
}

std::vector<std::string> AudioOutputService::getAvailableOutputs() const {
  return outputSwitcher->getAvailableOutputs();
}
