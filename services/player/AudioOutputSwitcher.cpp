#include "services/player/AudioOutputSwitcher.h"
#include "services/system/AlsaMixer.h"
#include <iostream>

AudioOutputSwitcher::AudioOutputSwitcher() { detectCurrentOutput(); }

bool AudioOutputSwitcher::switchToSpeakers() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (AlsaMixer::getInstance().switchToSpeakers()) {
    currentOutput_ = "speakers";
    std::cout << "[AudioOutputSwitcher] Switched to speakers" << std::endl;
    return true;
  }
  return false;
}

bool AudioOutputSwitcher::switchToHeadphones() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (AlsaMixer::getInstance().switchToHeadphones()) {
    currentOutput_ = "headphones";
    std::cout << "[AudioOutputSwitcher] Switched to headphones" << std::endl;
    return true;
  }
  return false;
}

std::string AudioOutputSwitcher::getCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string realOutput = AlsaMixer::getInstance().getCurrentOutput();
  if (realOutput != currentOutput_) {
    std::cout << "[AudioOutputSwitcher] State mismatch - real: " << realOutput
              << ", cached: " << currentOutput_ << std::endl;
    currentOutput_ = realOutput;
  }
  return currentOutput_;
}

std::vector<std::string> AudioOutputSwitcher::getAvailableOutputs() const {
  // getAvailableOutputs() в AlsaMixer - не статический, нужен экземпляр
  return AlsaMixer::getInstance().getAvailableOutputs();
}

void AudioOutputSwitcher::detectCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex_);
  currentOutput_ = AlsaMixer::getInstance().getCurrentOutput();
  std::cout << "[AudioOutputSwitcher] Initial output: " << currentOutput_
            << std::endl;
}
