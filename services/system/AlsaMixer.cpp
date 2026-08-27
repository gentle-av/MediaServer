#include "AlsaMixer.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <regex>
#include <sys/select.h>
#include <unistd.h>

const std::vector<std::string> AlsaMixer::availableOutputs = {"speakers",
                                                              "headphones"};

AlsaMixer::AlsaMixer()
    : controlName("Master"), currentVolume(30), muted(false),
      currentOutput("speakers"), initialized(false), initAttempted(false) {}

AlsaMixer::~AlsaMixer() {}

AlsaMixer &AlsaMixer::getInstance() {
  static AlsaMixer instance;
  return instance;
}

bool AlsaMixer::init() {
  if (initialized)
    return true;
  if (initAttempted)
    return false;
  initAttempted = true;
  std::string testOutput;
  if (!executeAmixer("sget Master 2>/dev/null", testOutput)) {
    std::cerr << "[AlsaMixer] amixer test failed" << std::endl;
    return false;
  }
  initialized = true;
  try {
    detectCurrentOutput();
    getVolume();
  } catch (...) {
    std::cerr << "[AlsaMixer] Warning: Failed to initialize ALSA mixer"
              << std::endl;
  }
  return initialized;
}

bool AlsaMixer::executeAmixer(const std::string &command, std::string &output) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  std::string fullCmd = "timeout 1 amixer " + command + " 2>&1";
  FILE *pipe = popen(fullCmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "[AlsaMixer] Failed to execute: " << fullCmd << std::endl;
    return false;
  }
  std::array<char, 512> buffer;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }
  int exitCode = pclose(pipe);
  if (exitCode != 0) {
    std::cerr << "[AlsaMixer] Command failed with code " << exitCode << ": "
              << fullCmd << std::endl;
    return false;
  }
  if (output.find("No such") != std::string::npos ||
      output.find("Invalid") != std::string::npos ||
      output.find("error") != std::string::npos) {
    std::cerr << "[AlsaMixer] Command returned error: " << output << std::endl;
    return false;
  }
  return true;
}

int AlsaMixer::parseVolumeFromOutput(const std::string &output) {
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

int AlsaMixer::getVolume() {
  if (!initialized)
    return currentVolume;
  std::lock_guard<std::mutex> lock(mutex);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName + " 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return currentVolume;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  int volume = parseVolumeFromOutput(result);
  if (volume >= 0) {
    currentVolume = volume;
  }
  return volume >= 0 ? volume : currentVolume;
}

bool AlsaMixer::setVolume(int percent) {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  std::string output;
  std::string cmd = "sset " + controlName + " " + std::to_string(percent) + "%";
  if (executeAmixer(cmd, output)) {
    currentVolume = percent;
    return true;
  }
  return false;
}

bool AlsaMixer::increaseVolume(int delta) {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  if (delta <= 0)
    return false;
  std::string output;
  std::string cmd = "sset " + controlName + " " + std::to_string(delta) + "%+";
  if (executeAmixer(cmd, output)) {
    currentVolume = getVolume();
    return true;
  }
  return false;
}

bool AlsaMixer::decreaseVolume(int delta) {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  if (delta <= 0)
    return false;
  std::string output;
  std::string cmd = "sset " + controlName + " " + std::to_string(delta) + "%-";
  if (executeAmixer(cmd, output)) {
    currentVolume = getVolume();
    return true;
  }
  return false;
}

bool AlsaMixer::toggleMute() {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd = "sset " + controlName + " toggle";
  if (executeAmixer(cmd, output)) {
    muted = !muted;
    return true;
  }
  return false;
}

bool AlsaMixer::isMuted() {
  if (!initialized)
    return muted;
  std::lock_guard<std::mutex> lock(mutex);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName + " 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return muted;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  std::regex mutedPattern(R"(\[off\])");
  muted = std::regex_search(result, mutedPattern);
  return muted;
}

std::string AlsaMixer::getControlName() { return controlName; }

bool AlsaMixer::switchToSpeakers() {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd = "sset 'Analog Output' Speakers 2>/dev/null";
  if (executeAmixer(cmd, output)) {
    currentOutput = "speakers";
    detectCurrentOutput();
    std::cout << "[AlsaMixer] Switched to speakers" << std::endl;
    return true;
  }
  // Если не получилось, пробуем другой вариант
  cmd = "sset 'Analog Output' 'Speakers' 2>/dev/null";
  if (executeAmixer(cmd, output)) {
    currentOutput = "speakers";
    detectCurrentOutput();
    std::cout << "[AlsaMixer] Switched to speakers" << std::endl;
    return true;
  }
  return false;
}

bool AlsaMixer::switchToHeadphones() {
  if (!initialized)
    return false;
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd = "sset 'Analog Output' Headphones 2>/dev/null";
  if (executeAmixer(cmd, output)) {
    currentOutput = "headphones";
    detectCurrentOutput();
    std::cout << "[AlsaMixer] Switched to headphones" << std::endl;
    return true;
  }
  cmd = "sset 'Analog Output' 'Headphones' 2>/dev/null";
  if (executeAmixer(cmd, output)) {
    currentOutput = "headphones";
    detectCurrentOutput();
    std::cout << "[AlsaMixer] Switched to headphones" << std::endl;
    return true;
  }
  return false;
}

std::string AlsaMixer::getCurrentOutput() {
  if (!initialized)
    return "speakers";
  std::lock_guard<std::mutex> lock(mutex);
  detectCurrentOutput();
  return currentOutput;
}

std::vector<std::string> AlsaMixer::getAvailableOutputs() {
  return availableOutputs;
}

void AlsaMixer::detectCurrentOutput() {
  if (!initialized)
    return;
  std::array<char, 256> buffer;
  std::string result;
  // Пробуем разные варианты команды
  FILE *pipe = popen(
      "amixer -c 0 sget 'Analog Output' 2>/dev/null | grep 'Item0:' | head -1",
      "r");
  if (!pipe) {
    currentOutput = "speakers";
    return;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  if (result.find("Speakers") != std::string::npos) {
    currentOutput = "speakers";
  } else if (result.find("Headphones") != std::string::npos) {
    currentOutput = "headphones";
  } else {
    currentOutput = "speakers";
  }
}
