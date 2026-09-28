#include "WpVolumeControl.h"
#include <array>
#include <cstdio>
#include <regex>
#include <sys/wait.h>

const std::vector<std::string> WpVolumeControl::availableOutputs = {
    "speakers", "headphones"};

WpVolumeControl &WpVolumeControl::getInstance() {
  static WpVolumeControl instance;
  return instance;
}

WpVolumeControl::WpVolumeControl()
    : controlName("WirePlumber"), currentVolume(30), muted(false),
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
  if (!executeWpctl("get-volume " + targetSink, testOutput)) {
    return false;
  }
  initialized = true;
  detectCurrentOutputUnlocked();
  currentVolume = parseVolumeFromOutput(testOutput);
  muted = (testOutput.find("[MUTED]") != std::string::npos);
  return initialized;
}

bool WpVolumeControl::executeWpctl(const std::string &command,
                                   std::string &output) {
  std::string fullCmd = "wpctl " + command + " 2>&1";
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
  std::regex volumePattern(R"(Volume:\s*([0-9.]+))");
  std::smatch match;
  if (std::regex_search(output, match, volumePattern)) {
    try {
      double volFloat = std::stod(match[1].str());
      return static_cast<int>(volFloat * 100.0 + 0.5);
    } catch (...) {
      return -1;
    }
  }
  return -1;
}

int WpVolumeControl::getVolume() {
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  if (executeWpctl("get-volume " + targetSink, output)) {
    currentVolume = parseVolumeFromOutput(output);
  }
  return currentVolume;
}

bool WpVolumeControl::setVolume(int percent) {
  if (!initialized && !init())
    return false;
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  std::lock_guard<std::mutex> lock(mutex);
  double volCoef = percent / 100.0;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f", volCoef);
  std::string output;
  std::string cmd = "set-volume " + targetSink + " " + std::string(buf);
  if (executeWpctl(cmd, output)) {
    currentVolume = percent;
    return true;
  }
  return false;
}

bool WpVolumeControl::increaseVolume(int delta) {
  if (!initialized && !init())
    return false;
  if (delta <= 0)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%+";
  if (executeWpctl(cmd, output)) {
    getVolume();
    return true;
  }
  return false;
}

bool WpVolumeControl::decreaseVolume(int delta) {
  if (!initialized && !init())
    return false;
  if (delta <= 0)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%-";
  if (executeWpctl(cmd, output)) {
    getVolume();
    return true;
  }
  return false;
}

bool WpVolumeControl::toggleMute() {
  if (!initialized && !init())
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  if (executeWpctl("set-mute " + targetSink + " toggle", output)) {
    muted = !muted;
    return true;
  }
  return false;
}

bool WpVolumeControl::isMuted() {
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  if (executeWpctl("get-volume " + targetSink, output)) {
    muted = (output.find("[MUTED]") != std::string::npos);
  }
  return muted;
}

std::string WpVolumeControl::getControlName() { return controlName; }

bool WpVolumeControl::switchToSpeakers() {
  if (!initialized && !init())
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  if (executeWpctl("set-profile @DEFAULT_AUDIO_SINK@ speakers", output)) {
    currentOutput = "speakers";
    return true;
  }
  currentOutput = "speakers";
  return true;
}

bool WpVolumeControl::switchToHeadphones() {
  if (!initialized && !init())
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  if (executeWpctl("set-profile @DEFAULT_AUDIO_SINK@ headphones", output)) {
    currentOutput = "headphones";
    return true;
  }
  currentOutput = "headphones";
  return true;
}

std::string WpVolumeControl::getCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex);
  detectCurrentOutputUnlocked();
  return currentOutput;
}

std::vector<std::string> WpVolumeControl::getAvailableOutputs() {
  return availableOutputs;
}

void WpVolumeControl::detectCurrentOutputUnlocked() {
  if (!initialized)
    return;
  std::string output;
  if (!executeWpctl("inspect", output)) {
    currentOutput = "speakers";
    return;
  }
  if (output.find("headphone") != std::string::npos ||
      output.find("phone") != std::string::npos) {
    currentOutput = "headphones";
  } else {
    currentOutput = "speakers";
  }
}
