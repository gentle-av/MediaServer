#include "AudioOutputService.h"
#include "../../services/system/WpVolumeControl.h"
#include <iostream>

AudioOutputService::AudioOutputService() {
  std::cout << "[AudioOutputService::ctor] constructed" << std::endl;
  outputSwitcher = std::make_unique<AudioOutputSwitcher>();
}

int AudioOutputService::getVolume() const {
  return WpVolumeControl::getInstance().getVolume();
}

void AudioOutputService::setVolume(int percent) {
  WpVolumeControl::getInstance().setVolume(percent);
}

void AudioOutputService::adjustVolume(int delta) {
  if (delta > 0) {
    WpVolumeControl::getInstance().increaseVolume(delta);
  } else {
    WpVolumeControl::getInstance().decreaseVolume(-delta);
  }
}

void AudioOutputService::toggleMute() {
  WpVolumeControl::getInstance().toggleMute();
}

bool AudioOutputService::isMuted() const {
  return WpVolumeControl::getInstance().isMuted();
}

bool AudioOutputService::switchToSpeakers() {
  std::cout << "[AudioOutputService::switchToSpeakers] ENTER" << std::endl;
  bool ok = outputSwitcher->switchToSpeakers();
  std::cout << "[AudioOutputService::switchToSpeakers] result=" << ok
            << std::endl;
  return ok;
}

bool AudioOutputService::switchToHeadphones() {
  std::cout << "[AudioOutputService::switchToHeadphones] ENTER" << std::endl;
  bool ok = outputSwitcher->switchToHeadphones();
  std::cout << "[AudioOutputService::switchToHeadphones] result=" << ok
            << std::endl;
  return ok;
}

std::string AudioOutputService::getCurrentOutput() const {
  return outputSwitcher->getCurrentOutput();
}

std::vector<std::string> AudioOutputService::getAvailableOutputs() const {
  return outputSwitcher->getAvailableOutputs();
}
