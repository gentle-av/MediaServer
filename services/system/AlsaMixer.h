#pragma once

#include <mutex>
#include <string>
#include <vector>

class AlsaMixer {
public:
  static AlsaMixer &getInstance();
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
  AlsaMixer();
  ~AlsaMixer();

  AlsaMixer(const AlsaMixer &) = delete;
  AlsaMixer &operator=(const AlsaMixer &) = delete;

  bool executeAmixer(const std::string &command, std::string &output);
  int parseVolumeFromOutput(const std::string &output);
  void detectCurrentOutput();
  bool init();
  std::mutex mutex;
  std::string controlName;
  int currentVolume;
  bool muted;
  std::string currentOutput;
  bool initialized;
  bool initAttempted;
  static const std::vector<std::string> availableOutputs;
};
