#pragma once

#include <json/json.h>

class Volumer {
public:
  int getVolume() const;
  bool setVolume(int volume);
  void increaseVolume();
  void decreaseVolume();
  void toggleMute();

private:
  int amixerValueToPercent(int amixerValue) const;
  int percentToAmixerValue(int percent) const;
  static constexpr int MIN_AMIXER = 135;
  static constexpr int MAX_AMIXER = 255;
};
