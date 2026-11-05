#include "WpVolumeControl.h"
#include <array>
#include <cstdio>
#include <future>
#include <iostream>
#include <regex>
#include <sys/wait.h>

const std::vector<std::string> WpVolumeControl::availableOutputs = {
    "speakers", "headphones"};

WpVolumeControl &WpVolumeControl::getInstance() {
  static WpVolumeControl instance;
  return instance;
}

WpVolumeControl::WpVolumeControl()
    : controlName("Master"), currentVolume(30), muted(false),
      currentOutput("speakers"), initialized(false), initAttempted(false),
      targetSink("@DEFAULT_AUDIO_SINK@") {
  init();
}

bool WpVolumeControl::init() {
  if (initialized)
    return true;
  if (initAttempted)
    return false;

  initAttempted = true;

  std::string testOutput;
  if (!executeWpctl("sget Master", testOutput)) {
    return false;
  }

  initialized = true;
  detectCurrentOutputUnlocked();
  currentVolume = parseVolumeFromOutput(testOutput);
  muted = (testOutput.find("[off]") != std::string::npos);

  return initialized;
}

bool WpVolumeControl::executeWpctl(const std::string &command,
                                   std::string &output) {
  std::string fullCmd = "timeout 1 amixer " + command + " 2>&1";
  FILE *pipe = popen(fullCmd.c_str(), "r");
  if (!pipe) {
    return false;
  }

  std::array<char, 256> buffer;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }

  int status = pclose(pipe);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int WpVolumeControl::parseVolumeFromOutput(const std::string &output) {
  std::regex volumePattern(R"(\[(\d{1,3})%\])");
  std::smatch match;

  if (std::regex_search(output, match, volumePattern)) {
    try {
      return std::stoi(match[1].str());
    } catch (...) {
      return -1;
    }
  }
  return -1;
}

int WpVolumeControl::getVolume() {
  std::string volOutput;
  if (executeWpctl("sget Master", volOutput)) {
    int volume = parseVolumeFromOutput(volOutput);
    if (volume >= 0) {
      currentVolume = volume;
    }
  }
  return currentVolume;
}

bool WpVolumeControl::setVolume(int percent) {
  if (!initialized && !init()) {
    return false;
  }
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;

  std::lock_guard<std::mutex> lock(mutex);
  std::string cmd = "sset Master " + std::to_string(percent) + "%";
  std::string output;
  if (executeWpctl(cmd, output)) {
    currentVolume = percent;
    return true;
  }
  return false;
}

bool WpVolumeControl::increaseVolume(int delta) {
  if (!initialized && !init()) {
    return false;
  }
  if (delta <= 0)
    return false;

  std::lock_guard<std::mutex> lock(mutex);
  std::string cmd = "sset Master " + std::to_string(delta) + "%+";
  std::string output;
  if (executeWpctl(cmd, output)) {
    std::string volOutput;
    executeWpctl("sget Master", volOutput);
    currentVolume = parseVolumeFromOutput(volOutput);
    return true;
  }
  return false;
}

bool WpVolumeControl::decreaseVolume(int delta) {
  if (!initialized && !init()) {
    return false;
  }
  if (delta <= 0)
    return false;

  std::lock_guard<std::mutex> lock(mutex);
  std::string cmd = "sset Master " + std::to_string(delta) + "%-";
  std::string output;
  if (executeWpctl(cmd, output)) {
    std::string volOutput;
    executeWpctl("sget Master", volOutput);
    currentVolume = parseVolumeFromOutput(volOutput);
    return true;
  }
  return false;
}

bool WpVolumeControl::toggleMute() {
  if (!initialized && !init()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex);
  std::string cmd = "sset Master toggle";
  std::string output;
  if (executeWpctl(cmd, output)) {
    muted = (output.find("[off]") != std::string::npos);
    return true;
  }
  return false;
}

bool WpVolumeControl::isMuted() {
  std::string amixerOutput;
  if (executeWpctl("sget Master", amixerOutput)) {
    muted = (amixerOutput.find("[off]") != std::string::npos);
  }
  return muted;
}

std::string WpVolumeControl::getControlName() { return controlName; }

bool WpVolumeControl::switchToSpeakers() {
  if (!initialized && !init()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex);
  std::string result;
  bool ok = executeWpctl("-c 0 cset numid=22 Speakers", result);
  std::cerr << "[WpVolumeControl::switchToSpeakers] ALSA direct write result="
            << ok << " output='" << result << "'" << std::endl;

  if (ok) {
    currentOutput = "speakers";
  }
  return ok;
}

bool WpVolumeControl::switchToHeadphones() {
  if (!initialized && !init()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex);
  std::string result;
  bool ok = executeWpctl("-c 0 cset numid=22 Headphones", result);
  std::cerr << "[WpVolumeControl::switchToHeadphones] ALSA direct write result="
            << ok << " output='" << result << "'" << std::endl;

  if (ok) {
    currentOutput = "headphones";
  }
  return ok;
}

std::string WpVolumeControl::getCurrentOutput() {
  detectCurrentOutputUnlocked();
  return currentOutput;
}

std::vector<std::string> WpVolumeControl::getAvailableOutputs() {
  return availableOutputs;
}

void WpVolumeControl::detectCurrentOutputUnlocked() {
  if (!initialized)
    return;

  std::string amixerOutput;
  std::array<char, 256> buffer;
  FILE *pipe = popen("amixer -c 0 cget numid=22 2>&1", "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      amixerOutput += buffer.data();
    }
    pclose(pipe);
  }

  size_t pos = amixerOutput.find(": values=");
  if (pos == std::string::npos) {
    currentOutput = "speakers";
    return;
  }

  pos += 9;
  if (pos < amixerOutput.size()) {
    char digit = amixerOutput[pos];
    // Для Xonar STX II: values=1 (Headphones), values=2 (FP Headphones),
    // values=0 (Speakers)
    if (digit == '1' || digit == '2') {
      currentOutput = "headphones";
    } else {
      currentOutput = "speakers";
    }
    std::cerr << "[WpVolumeControl::detectCurrentOutputUnlocked] ALSA active "
                 "path value digit='"
              << digit << "' -> currentOutput='" << currentOutput << "'"
              << std::endl;
  }
}
