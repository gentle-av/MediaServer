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
  bool executeAmixer(const std::string &command);
  int parseVolumeFromOutput(const std::string &output);
  std::string getCurrentOutputInternal();
  void detectCurrentOutput();

  std::mutex mutex_;
  std::string controlName_;
  int currentVolume_;
  bool muted_;
  std::string currentOutput_;
  static const std::vector<std::string> availableOutputs_;
};
