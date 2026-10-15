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

int WpVolumeControl::getVolume() { return currentVolume; }

bool WpVolumeControl::setVolume(int percent) {
  if (!initialized && !init()) {
    return false;
  }
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 100) {
    percent = 100;
  }
  double volCoef = percent / 100.0;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f", volCoef);
  currentVolume = percent;
  std::string cmd = "set-volume " + targetSink + " " + std::string(buf);
  std::thread([this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    this->executeWpctl(cmd, output);
  }).detach();
  return true;
}

bool WpVolumeControl::increaseVolume(int delta) {
  if (!initialized && !init()) {
    return false;
  }
  if (delta <= 0) {
    return false;
  }
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%+";
  std::thread([this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    if (this->executeWpctl(cmd, output)) {
      std::string volOutput;
      this->executeWpctl("get-volume " + this->targetSink, volOutput);
      this->currentVolume = this->parseVolumeFromOutput(volOutput);
    }
  }).detach();
  return true;
}

bool WpVolumeControl::decreaseVolume(int delta) {
  if (!initialized && !init()) {
    return false;
  }
  if (delta <= 0) {
    return false;
  }
  std::string cmd =
      "set-volume " + targetSink + " " + std::to_string(delta) + "%-";
  std::thread([this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    if (this->executeWpctl(cmd, output)) {
      std::string volOutput;
      this->executeWpctl("get-volume " + this->targetSink, volOutput);
      this->currentVolume = this->parseVolumeFromOutput(volOutput);
    }
  }).detach();
  return true;
}

bool WpVolumeControl::toggleMute() {
  if (!initialized && !init()) {
    return false;
  }
  muted = !muted;
  std::string cmd = "set-mute " + targetSink + " toggle";
  std::thread([this, cmd]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string output;
    this->executeWpctl(cmd, output);
  }).detach();
  return true;
}

bool WpVolumeControl::isMuted() { return muted; }

std::string WpVolumeControl::getControlName() { return controlName; }

bool WpVolumeControl::switchToSpeakers() {
  if (!initialized && !init()) {
    return false;
  }
  currentOutput = "speakers";
  std::thread([this]() {
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
  }).detach();
  return true;
}

bool WpVolumeControl::switchToHeadphones() {
  if (!initialized && !init()) {
    return false;
  }
  currentOutput = "headphones";
  std::thread([this]() {
    std::lock_guard<std::mutex> lock(mutex);
    std::array<char, 256> buffer;
    std::string result;
    FILE *pipe = popen("amixer -c 0 cset numid=22 Headphones 2>&1", "r");
    if (pipe) {
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
      }
      int status = pclose(pipe);
      int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
      std::cerr << "[WpVolumeControl::switchToHeadphones] exit=" << exitCode
                << " output='" << result << "'" << std::endl;
    } else {
      std::cerr << "[WpVolumeControl::switchToHeadphones] popen failed"
                << std::endl;
    }
  }).detach();
  return true;
}

std::string WpVolumeControl::getCurrentOutput() { return currentOutput; }

std::vector<std::string> WpVolumeControl::getAvailableOutputs() {
  return availableOutputs;
}

void WpVolumeControl::detectCurrentOutputUnlocked() {
  if (!initialized)
    return;

  std::string amixerOutput;
  {
    std::array<char, 256> buffer;
    FILE *pipe = popen("amixer -c 0 cget numid=22 2>&1", "r");
    if (pipe) {
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        amixerOutput += buffer.data();
      }
      pclose(pipe);
    }
  }

  std::cerr << "[WpVolumeControl::detectCurrentOutputUnlocked] amixerOutput='"
            << amixerOutput << "'" << '\n';

  size_t pos = amixerOutput.find(": values=");
  if (pos == std::string::npos) {
    std::cerr << "[WpVolumeControl::detectCurrentOutputUnlocked] "
                 "': values=' not found, fallback to wpctl"
              << '\n';
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
    return;
  }

  pos += 9;
  if (pos < amixerOutput.size()) {
    char digit = amixerOutput[pos];
    if (digit == '1' || digit == '2') {
      currentOutput = "headphones";
    } else {
      currentOutput = "speakers";
    }
    std::cerr << "[WpVolumeControl::detectCurrentOutputUnlocked] digit='"
              << digit << "' -> currentOutput='" << currentOutput << "'"
              << '\n';
  }
}
