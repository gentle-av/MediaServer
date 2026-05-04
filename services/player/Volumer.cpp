#include "services/player/Volumer.h"
#include <array>
#include <cstdio>
#include <regex>
#include <string>

int Volumer::getVolume() const {
  std::string cmd = "timeout 2 amixer get Master 2>/dev/null";
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return -1;
  while (fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  pclose(pipe);
  std::regex volumeRegex(R"((\d+)%)");
  std::smatch match;
  if (std::regex_search(result, match, volumeRegex))
    return std::stoi(match[1].str());
  return -1;
}

bool Volumer::setVolume(int volume) {
  if (volume < 0 || volume > 100)
    return false;
  int amixerValue = percentToAmixerValue(volume);
  std::string cmd = "timeout 2 amixer set Master " +
                    std::to_string(amixerValue) + " 2>/dev/null";
  return system(cmd.c_str()) == 0;
}

void Volumer::increaseVolume() {
  system("timeout 2 amixer set Master 5%+ 2>/dev/null");
}

void Volumer::decreaseVolume() {
  system("timeout 2 amixer set Master 5%- 2>/dev/null");
}

void Volumer::toggleMute() {
  system("timeout 2 amixer set Master toggle 2>/dev/null");
}

int Volumer::amixerValueToPercent(int amixerValue) const {
  return (amixerValue - MIN_AMIXER) * 100 / (MAX_AMIXER - MIN_AMIXER);
}

int Volumer::percentToAmixerValue(int percent) const {
  return MIN_AMIXER + (percent * (MAX_AMIXER - MIN_AMIXER) / 100);
}
