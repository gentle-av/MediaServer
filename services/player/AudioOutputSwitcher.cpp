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
  std::cout << "[AudioOutputSwitcher::switchToSpeakers] ENTER" << std::endl;
  bool ok = WpVolumeControl::getInstance().switchToSpeakers();
  std::cout << "[AudioOutputSwitcher::switchToSpeakers] Wpctl returned " << ok
            << std::endl;
  if (ok) {
    currentOutput = "speakers";
    return true;
  }
  return false;
}

bool AudioOutputSwitcher::switchToHeadphones() {
  std::cout << "[AudioOutputSwitcher::switchToHeadphones] ENTER" << std::endl;
  bool ok = WpVolumeControl::getInstance().switchToHeadphones();
  std::cout << "[AudioOutputSwitcher::switchToHeadphones] Wpctl returned " << ok
            << std::endl;
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
