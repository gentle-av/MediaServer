#include "WpVolumeControl.h"
#include <array>
#include <cstdio>
#include <future>
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
  double volCoef = percent / 100.0;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f", volCoef);
  currentVolume = percent;
  std::string cmd = "set-volume " + targetSink + " " + std::string(buf);
  auto some_future = std::async(std::launch::async, [this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    this->executeWpctl(cmd, output);
  });
  return true;
}

bool WpVolumeControl::increaseVolume(int delta) {
  if (!initialized && !init())
    return false;
  if (delta <= 0)
    return false;
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%+";
  auto some_future = std::async(std::launch::async, [this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    if (this->executeWpctl(cmd, output)) {
      std::string volOutput;
      this->executeWpctl("get-volume " + this->targetSink, volOutput);
      this->currentVolume = this->parseVolumeFromOutput(volOutput);
    }
  });
  return true;
}

bool WpVolumeControl::decreaseVolume(int delta) {
  if (!initialized && !init())
    return false;
  if (delta <= 0)
    return false;
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%-";
  auto some_future = std::async(std::launch::async, [this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    if (this->executeWpctl(cmd, output)) {
      std::string volOutput;
      this->executeWpctl("get-volume " + this->targetSink, volOutput);
      this->currentVolume = this->parseVolumeFromOutput(volOutput);
    }
  });
  return true;
}

bool WpVolumeControl::toggleMute() {
  if (!initialized && !init())
    return false;
  muted = !muted;
  std::string cmd = "set-mute " + targetSink + " toggle";
  auto some_future = std::async(std::launch::async, [this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    this->executeWpctl(cmd, output);
  });
  return true;
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
  currentOutput = "speakers";
  auto some_future = std::async(std::launch::async, [this]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen("amixer -c 0 cset numid=22 Speakers 2>&1", "r");
    if (pipe) {
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
      }
      pclose(pipe);
    }
  });
  return true;
}

bool WpVolumeControl::switchToHeadphones() {
  if (!initialized && !init())
    return false;
  currentOutput = "headphones";
  auto some_future = std::async(std::launch::async, [this]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen("amixer -c 0 cset numid=22 Headphones 2>&1", "r");
    if (pipe) {
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
      }
      pclose(pipe);
    }
  });
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
