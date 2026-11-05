#include "AlsaMixer.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <regex>
#include <sys/select.h>
#include <sys/wait.h>
#include <thread>
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
  if (!runAmixerRaw("-c II sget Master", testOutput))
    return false;
  initialized = true;
  try {
    std::array<char, 512> buffer;
    std::string result;
    FILE *pipe = popen("amixer -c II sget Master 2>&1", "r");
    if (pipe) {
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
      }
      pclose(pipe);
      int volume = parseVolumeFromOutput(result);
      if (volume >= 0)
        currentVolume = volume;
      std::regex mutedPattern(R"(\[off\])");
      muted = std::regex_search(result, mutedPattern);
    }
    std::string out;
    if (runAmixerRaw("-c II cget name='Analog Output'", out)) {
      std::transform(out.begin(), out.end(), out.begin(), ::tolower);
      if (out.find("headphone") != std::string::npos ||
          out.find("values=1") != std::string::npos ||
          out.find("values=2") != std::string::npos) {
        currentOutput = "headphones";
      } else {
        currentOutput = "speakers";
      }
    }
  } catch (...) {
  }
  return initialized;
}

bool AlsaMixer::runAmixerRaw(const std::string &command, std::string &output) {
  std::string fullCmd = "timeout 1 amixer " + command + " 2>&1";
  FILE *pipe = popen(fullCmd.c_str(), "r");
  if (!pipe)
    return false;
  std::array<char, 512> buffer;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }
  int status = pclose(pipe);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool AlsaMixer::executeAmixer(const std::string &command, std::string &output) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  return runAmixerRaw(command, output);
}

int AlsaMixer::parseVolumeFromOutput(const std::string &output) {
  std::regex volumePattern(R"(\[(\d{1,3})%\])");
  std::smatch match;
  if (std::regex_search(output, match, volumePattern)) {
    try {
      return std::stoi(match.str());
    } catch (...) {
      return -1;
    }
  }
  return -1;
}

int AlsaMixer::getVolume() { return currentVolume; }

bool AlsaMixer::setVolume(int percent) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  std::string output;
  std::string cmd =
      "-c II sset " + controlName + " " + std::to_string(percent) + "%";
  if (executeAmixer(cmd, output)) {
    currentVolume = percent;
    return true;
  }
  return false;
}

bool AlsaMixer::increaseVolume(int delta) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  if (delta <= 0)
    return false;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  int targetVolume = std::min(100, currentVolume + delta);
  std::string output;
  std::string cmd =
      "-c II sset " + controlName + " " + std::to_string(targetVolume) + "%";
  if (executeAmixer(cmd, output)) {
    currentVolume = targetVolume;
    return true;
  }
  return false;
}

bool AlsaMixer::decreaseVolume(int delta) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  if (delta <= 0)
    return false;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  int targetVolume = std::max(0, currentVolume - delta);
  std::string output;
  std::string cmd =
      "-c II sset " + controlName + " " + std::to_string(targetVolume) + "%";
  if (executeAmixer(cmd, output)) {
    currentVolume = targetVolume;
    return true;
  }
  return false;
}

bool AlsaMixer::toggleMute() {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  std::string output;
  std::string cmd = "-c II sset " + controlName + " toggle";
  if (executeAmixer(cmd, output)) {
    muted = !muted;
    return true;
  }
  return false;
}

bool AlsaMixer::isMuted() { return muted; }

std::string AlsaMixer::getControlName() { return controlName; }

bool AlsaMixer::switchToSpeakers() {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  std::string output;
  if (executeAmixer("-c II cset name='Analog Output' 0", output)) {
    currentOutput = "speakers";
    return true;
  }
  return false;
}

bool AlsaMixer::switchToHeadphones() {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  std::lock_guard<std::recursive_mutex> lock(mutex);
  std::string output;
  if (executeAmixer("-c II cset name='Analog Output' 1", output)) {
    currentOutput = "headphones";
    return true;
  }
  return false;
}

std::string AlsaMixer::getCurrentOutput() { return currentOutput; }

std::vector<std::string> AlsaMixer::getAvailableOutputs() {
  return availableOutputs;
}

void AlsaMixer::detectCurrentOutput() {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  detectCurrentOutputUnlocked();
}

void AlsaMixer::detectCurrentOutputUnlocked() {}
