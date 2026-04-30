#pragma once

#include <mutex>
#include <string>

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

private:
  AlsaMixer();
  ~AlsaMixer();
  AlsaMixer(const AlsaMixer &) = delete;
  AlsaMixer &operator=(const AlsaMixer &) = delete;

  bool executeAmixer(const std::string &command);
  int parseVolumeFromOutput(const std::string &output);

  std::mutex mutex_;
  std::string controlName_;
  int currentVolume_;
  bool muted_;
};
