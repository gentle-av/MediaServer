#include "services/player/AudioOutputSwitcher.h"
#include "services/system/WpVolumeControl.h"
#include <iostream>

AudioOutputSwitcher::AudioOutputSwitcher() {
  std::cout << "[AudioOutputSwitcher::ctor] constructed" << std::endl;
  int vol = WpVolumeControl::getInstance().getVolume();
  std::cout << "[AudioOutputSwitcher::ctor] getVolume() returned " << vol
            << std::endl;
  if (vol >= 0) {
    detectCurrentOutput();
  }
}

bool AudioOutputSwitcher::switchToSpeakers() {
  bool ok = WpVolumeControl::getInstance().switchToSpeakers();
  if (ok) {
    currentOutput = "speakers";
    return true;
  }
  return false;
}

bool AudioOutputSwitcher::switchToHeadphones() {
  bool ok = WpVolumeControl::getInstance().switchToHeadphones();
  if (ok) {
    currentOutput = "headphones";
    return true;
  }
  return false;
}

std::string AudioOutputSwitcher::getCurrentOutput() {
  std::string realOutput = WpVolumeControl::getInstance().getCurrentOutput();
  if (realOutput != currentOutput) {
    std::cout << "[AudioOutputSwitcher] State mismatch - real: " << realOutput
              << ", cached: " << currentOutput << std::endl;
    currentOutput = realOutput;
  }
  return currentOutput;
}

std::vector<std::string> AudioOutputSwitcher::getAvailableOutputs() const {
  return WpVolumeControl::getInstance().getAvailableOutputs();
}

void AudioOutputSwitcher::detectCurrentOutput() {
  currentOutput = WpVolumeControl::getInstance().getCurrentOutput();
  std::cout << "[AudioOutputSwitcher] Initial output: " << currentOutput
            << std::endl;
}
