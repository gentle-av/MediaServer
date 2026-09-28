#pragma once

#include <mutex>
#include <string>
#include <vector>

class WpVolumeControl {
public:
  static WpVolumeControl &getInstance();
  int getVolume();
  bool setVolume(int percent);
  bool increaseVolume(int delta);
  bool decreaseVolume(int delta);
  bool toggleMute();
  bool isMuted();
  std::string getControlName();
  bool switchToSpeakers();
  bool switchToHeadphones();
  std::string getCurrentOutput();
  std::vector<std::string> getAvailableOutputs();

private:
  WpVolumeControl();
  ~WpVolumeControl() = default;

  WpVolumeControl(const WpVolumeControl &) = delete;
  WpVolumeControl &operator=(const WpVolumeControl &) = delete;

  bool executeWpctl(const std::string &command, std::string &output);
  int parseVolumeFromOutput(const std::string &output);
  void detectCurrentOutputUnlocked();
  bool init();

  std::mutex mutex;
  std::string controlName;
  int currentVolume;
  bool muted;
  std::string currentOutput;
  bool initialized;
  bool initAttempted;
  const std::string targetSink;
  static const std::vector<std::string> availableOutputs;
};
