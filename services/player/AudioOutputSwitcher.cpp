#include "services/player/AudioOutputSwitcher.h"
#include "services/system/AlsaMixer.h"
#include <iostream>

AudioOutputSwitcher::AudioOutputSwitcher() {
  std::cout << "[AudioOutputSwitcher::ctor] constructed" << std::endl;
  int vol = AlsaMixer::getInstance().getVolume();
  std::cout << "[AudioOutputSwitcher::ctor] getVolume() returned " << vol
            << std::endl;
  if (vol >= 0) {
    detectCurrentOutput();
  }
}

bool AudioOutputSwitcher::switchToSpeakers() {
  std::cout << "[AudioOutputSwitcher::switchToSpeakers] ENTER" << std::endl;
  std::lock_guard<std::mutex> lock(mutex);
  bool ok = AlsaMixer::getInstance().switchToSpeakers();
  std::cout << "[AudioOutputSwitcher::switchToSpeakers] AlsaMixer returned "
            << ok << std::endl;
  if (ok) {
    currentOutput = "speakers";
    return true;
  }
  return false;
}

bool AudioOutputSwitcher::switchToHeadphones() {
  std::cout << "[AudioOutputSwitcher::switchToHeadphones] ENTER" << std::endl;
  std::lock_guard<std::mutex> lock(mutex);
  bool ok = AlsaMixer::getInstance().switchToHeadphones();
  std::cout << "[AudioOutputSwitcher::switchToHeadphones] AlsaMixer returned "
            << ok << std::endl;
  if (ok) {
    currentOutput = "headphones";
    return true;
  }
  return false;
}

std::string AudioOutputSwitcher::getCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex);
  std::string realOutput = AlsaMixer::getInstance().getCurrentOutput();
  if (realOutput != currentOutput) {
    std::cout << "[AudioOutputSwitcher] State mismatch - real: " << realOutput
              << ", cached: " << currentOutput << std::endl;
    currentOutput = realOutput;
  }
  return currentOutput;
}

std::vector<std::string> AudioOutputSwitcher::getAvailableOutputs() const {
  return AlsaMixer::getInstance().getAvailableOutputs();
}

void AudioOutputSwitcher::detectCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex);
  currentOutput = AlsaMixer::getInstance().getCurrentOutput();
  std::cout << "[AudioOutputSwitcher] Initial output: " << currentOutput
            << std::endl;
}
