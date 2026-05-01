#include "AlsaMixer.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <regex>
#include <thread>
#include <unistd.h>

const std::vector<std::string> AlsaMixer::availableOutputs_ = {"speakers",
                                                               "headphones"};

bool AlsaMixer::switchToSpeakers() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::pair<std::string, std::string>> commands = {
      {"sset 'Analog Output'", "Speakers"}, {"sset 'Line'", "Line"}};
  for (const auto &cmd : commands) {
    std::string fullCmd = cmd.first + " " + cmd.second;
    if (executeAmixer(fullCmd)) {
      currentOutput_ = "speakers";
      detectCurrentOutput();
      return true;
    }
  }
  return false;
}

bool AlsaMixer::switchToHeadphones() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> commands = {"sset 'Analog Output' Headphones",
                                       "sset 'Headphones Impedance' 1"};
  for (const auto &cmd : commands) {
    if (executeAmixer(cmd)) {
      currentOutput_ = "headphones";
      detectCurrentOutput();
      return true;
    }
  }
  return false;
}

AlsaMixer::AlsaMixer()
    : controlName_("Master"), currentVolume_(0), muted_(false),
      currentOutput_("speakers") {
  detectCurrentOutput();
}

AlsaMixer::~AlsaMixer() {}

AlsaMixer &AlsaMixer::getInstance() {
  static AlsaMixer instance;
  return instance;
}

bool AlsaMixer::executeAmixer(const std::string &command) {
  std::string fullCmd = "amixer " + command + " 2>&1";
  FILE *pipe = popen(fullCmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "[AlsaMixer] Failed to execute: " << fullCmd << std::endl;
    return false;
  }
  int fd = fileno(pipe);
  if (fd != -1) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
  std::array<char, 512> buffer;
  auto startTime = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - startTime)
                       .count();
    if (elapsed > 3000) {
      std::cerr << "[AlsaMixer] Command timeout: " << fullCmd << std::endl;
      pclose(pipe);
      return false;
    }
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      continue;
    }
    if (feof(pipe))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  int exitCode = pclose(pipe);
  if (exitCode != 0) {
    std::cerr << "[AlsaMixer] Command failed with code " << exitCode << ": "
              << fullCmd << std::endl;
    return false;
  }
  return true;
}

int AlsaMixer::parseVolumeFromOutput(const std::string &output) {
  std::regex volumePattern(R"(\[(\d{1,3})%\])");
  std::smatch match;
  if (std::regex_search(output, match, volumePattern)) {
    return std::stoi(match[1].str());
  }
  return -1;
}

int AlsaMixer::getVolume() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName_ + " 2>/dev/null";
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe)
    return -1;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  int volume = parseVolumeFromOutput(result);
  if (volume >= 0)
    currentVolume_ = volume;
  return volume;
}

bool AlsaMixer::setVolume(int percent) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  std::string cmd =
      "sset " + controlName_ + " " + std::to_string(percent) + "%";
  if (executeAmixer(cmd)) {
    currentVolume_ = percent;
    return true;
  }
  return false;
}

bool AlsaMixer::increaseVolume(int delta) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (delta <= 0)
    return false;
  std::string cmd = "sset " + controlName_ + " " + std::to_string(delta) + "%+";
  if (executeAmixer(cmd)) {
    currentVolume_ = getVolume();
    return true;
  }
  return false;
}

bool AlsaMixer::decreaseVolume(int delta) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (delta <= 0)
    return false;
  std::string cmd = "sset " + controlName_ + " " + std::to_string(delta) + "%-";
  if (executeAmixer(cmd)) {
    currentVolume_ = getVolume();
    return true;
  }
  return false;
}

bool AlsaMixer::toggleMute() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string cmd = "sset " + controlName_ + " toggle";
  if (executeAmixer(cmd)) {
    muted_ = !muted_;
    return true;
  }
  return false;
}

bool AlsaMixer::isMuted() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName_ + " 2>/dev/null";
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe)
    return false;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  std::regex mutedPattern(R"(\[off\])");
  bool muted = std::regex_search(result, mutedPattern);
  muted_ = muted;
  return muted;
}

std::string AlsaMixer::getControlName() { return controlName_; }

std::string AlsaMixer::getCurrentOutput() {
  std::lock_guard<std::mutex> lock(mutex_);
  detectCurrentOutput();
  return currentOutput_;
}

std::vector<std::string> AlsaMixer::getAvailableOutputs() {
  return availableOutputs_;
}

void AlsaMixer::detectCurrentOutput() {
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(
      "amixer -c 0 get Headphone 2>/dev/null | grep -o 'on\\|off' | head -1",
      "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
  }
  if (!result.empty() && result.find("on") != std::string::npos) {
    currentOutput_ = "headphones";
    return;
  }
  pipe = popen(
      "amixer -c 0 get Speaker 2>/dev/null | grep -o 'on\\|off' | head -1",
      "r");
  if (pipe) {
    result.clear();
    while (fgets(buffer.data(), buffer.size(), pipe))
      result += buffer.data();
    pclose(pipe);
    if (!result.empty() && result.find("on") != std::string::npos) {
      currentOutput_ = "speakers";
      return;
    }
  }
  currentOutput_ = "speakers";
}
