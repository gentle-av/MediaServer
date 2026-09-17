#include "AlsaMixer.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <regex>
#include <sys/select.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

bool AlsaMixer::init() {
  if (initialized)
    return true;
  if (initAttempted)
    return false;
  initAttempted = true;
  std::string testOutput;
  if (!runAmixerRaw("sget Master", testOutput)) {
    std::cerr << "[AlsaMixer] amixer test failed. output='" << testOutput << "'"
              << std::endl;
    return false;
  }
  std::cerr << "[AlsaMixer] amixer test OK. output='" << testOutput << "'"
            << std::endl;
  initialized = true;
  try {
    detectCurrentOutput();
    getVolume();
  } catch (...) {
    std::cerr << "[AlsaMixer] Warning: exception in init" << std::endl;
  }
  return initialized;
}

bool AlsaMixer::runAmixerRaw(const std::string &command, std::string &output) {
  std::string fullCmd = "timeout 1 amixer " + command + " 2>&1";
  std::cerr << "[AlsaMixer] runAmixerRaw: fullCmd='" << fullCmd << "'"
            << std::endl;
  FILE *pipe = popen(fullCmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "[AlsaMixer] popen failed: " << fullCmd << std::endl;
    return false;
  }
  std::array<char, 512> buffer;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }
  int status = pclose(pipe);
  int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  std::cerr << "[AlsaMixer] raw status=" << status
            << " WIFEXITED=" << WIFEXITED(status) << " exitCode=" << exitCode
            << " output='" << output << "'" << std::endl;
  if (!WIFEXITED(status) || exitCode != 0) {
    std::cerr << "[AlsaMixer] command failed with exit=" << exitCode << ": "
              << fullCmd << std::endl;
    return false;
  }
  return true;
}

bool AlsaMixer::executeAmixer(const std::string &command, std::string &output) {
  if (!initAttempted)
    init();
  if (!initialized)
    return false;
  return runAmixerRaw(command, output);
}

const std::vector<std::string> AlsaMixer::availableOutputs = {"speakers",
                                                              "headphones"};

AlsaMixer::AlsaMixer()
    : controlName("Master"), currentVolume(30), muted(false),
      currentOutput("speakers"), initialized(false), initAttempted(false) {
  std::cerr << "[AlsaMixer::ctor] constructed (pid=" << ::getpid() << ")"
            << std::endl;
}

AlsaMixer::~AlsaMixer() {
  std::cerr << "[AlsaMixer::dtor] destroyed" << std::endl;
}

AlsaMixer &AlsaMixer::getInstance() {
  static AlsaMixer instance;
  return instance;
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
  std::cerr << "[AlsaMixer::getVolume] called" << std::endl;
  if (!initAttempted)
    init();
  if (!initialized)
    return currentVolume;
  std::lock_guard<std::mutex> lock(mutex);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName + " 2>&1";
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
  if (!initAttempted)
    init();
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
  if (!initAttempted)
    init();
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
  if (!initAttempted)
    init();
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
  if (!initAttempted)
    init();
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
  if (!initAttempted)
    init();
  if (!initialized)
    return muted;
  std::lock_guard<std::mutex> lock(mutex);
  std::array<char, 512> buffer;
  std::string result;
  std::string cmd = "amixer sget " + controlName + " 2>&1";
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
  std::cerr << "[AlsaMixer::switchToSpeakers] ENTER" << std::endl;
  if (!initAttempted) {
    init();
  }
  std::cerr << "[AlsaMixer::switchToSpeakers] initialized=" << initialized
            << std::endl;
  if (!initialized) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd = "-c 0 cset numid=22 Speakers";
  std::cerr << "[AlsaMixer::switchToSpeakers] cmd='" << cmd << "'" << std::endl;
  if (executeAmixer(cmd, output)) {
    currentOutput = "speakers";
    std::cerr << "[AlsaMixer::switchToSpeakers] SUCCESS" << std::endl;
    return true;
  }
  std::cerr << "[AlsaMixer::switchToSpeakers] FAILED. output='" << output << "'"
            << std::endl;
  return false;
}

bool AlsaMixer::switchToHeadphones() {
  std::cerr << "[AlsaMixer::switchToHeadphones] ENTER" << std::endl;
  if (!initAttempted) {
    init();
  }
  std::cerr << "[AlsaMixer::switchToHeadphones] initialized=" << initialized
            << std::endl;
  if (!initialized) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex);
  std::string output;
  std::string cmd = "-c 0 cset numid=22 Headphones";
  std::cerr << "[AlsaMixer::switchToHeadphones] cmd='" << cmd << "'"
            << std::endl;
  if (executeAmixer(cmd, output)) {
    currentOutput = "headphones";
    std::cerr << "[AlsaMixer::switchToHeadphones] SUCCESS" << std::endl;
    return true;
  }
  std::cerr << "[AlsaMixer::switchToHeadphones] FAILED. output='" << output
            << "'" << std::endl;
  return false;
}

std::string AlsaMixer::getCurrentOutput() {
  if (!initAttempted)
    init();
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
  std::cerr << "[AlsaMixer::detectCurrentOutput] ENTER. initialized="
            << initialized << std::endl;
  if (!initialized) {
    return;
  }
  std::string output;
  if (!executeAmixer("-c 0 cget numid=22", output)) {
    std::cerr << "[AlsaMixer::detectCurrentOutput] cget failed, default to "
                 "speakers"
              << std::endl;
    currentOutput = "speakers";
    return;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::string output2;
  if (executeAmixer("-c 0 cget numid=22", output2)) {
    output = output2;
  }
  if (output.find("values=1") != std::string::npos ||
      output.find("values=2") != std::string::npos) {
    currentOutput = "headphones";
  } else {
    currentOutput = "speakers";
  }
  std::cerr << "[AlsaMixer::detectCurrentOutput] currentOutput='"
            << currentOutput << "'" << std::endl;
}
