#include "services/player/AudioOutputSwitcher.h"
#include "services/system/AlsaMixer.h"
#include <iostream>

AudioOutputSwitcher::AudioOutputSwitcher() { detectCurrentOutput(); }

bool AudioOutputSwitcher::switchToSpeakers() {
  std::lock_guard<std::mutex> lock(mutex);
  if (AlsaMixer::getInstance().switchToSpeakers()) {
    currentOutput = "speakers";
    std::cout << "[AudioOutputSwitcher] Switched to speakers" << std::endl;
    return true;
  }
  return false;
}

bool AudioOutputSwitcher::switchToHeadphones() {
  std::lock_guard<std::mutex> lock(mutex);
  if (AlsaMixer::getInstance().switchToHeadphones()) {
    currentOutput = "headphones";
    std::cout << "[AudioOutputSwitcher] Switched to headphones" << std::endl;
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
