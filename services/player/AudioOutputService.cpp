#include "AudioOutputService.h"
#include "../../services/system/AlsaMixer.h"
#include <iostream>

AudioOutputService::AudioOutputService() {
  std::cout << "[AudioOutputService::ctor] Сквозная привязка к AlsaMixer"
            << std::endl;
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
  std::cout << "[AudioOutputService] Переключение на Speakers через ALSA"
            << std::endl;
  return AlsaMixer::getInstance().switchToSpeakers();
}

bool AudioOutputService::switchToHeadphones() {
  std::cout << "[AudioOutputService] Переключение на Headphones через ALSA"
            << std::endl;
  return AlsaMixer::getInstance().switchToHeadphones();
}

std::string AudioOutputService::getCurrentOutput() const {
  std::cout << AlsaMixer::getInstance().getCurrentOutput() << '\n';
  return AlsaMixer::getInstance().getCurrentOutput();
}

std::vector<std::string> AudioOutputService::getAvailableOutputs() const {
  return AlsaMixer::getInstance().getAvailableOutputs();
}
