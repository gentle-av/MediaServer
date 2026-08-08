#include "services/system/PowerService.h"
#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

static bool executeCommandWithTimeout(const std::vector<std::string> &args,
                                      std::string &output, int timeoutSec = 5) {
  if (args.empty())
    return false;
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[1]);
  char buffer[512];
  ssize_t n;
  auto start = std::chrono::steady_clock::now();
  while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    output += buffer;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed > timeoutSec) {
      kill(pid, SIGTERM);
      break;
    }
  }
  close(pipefd[0]);
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool executeCommandNoOutput(const std::vector<std::string> &args) {
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }
  if (pid == 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    std::vector<char *> argv;
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[0]);
  close(pipefd[1]);
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

PowerService::PowerService()
    : m_lastSleepCall(std::chrono::steady_clock::now()),
      m_isGoingToSleep(false), m_initialized(false) {
  if (access("/dev/snd/controlC0", R_OK) == 0) {
    m_initialized = true;
  }
}

PowerService::~PowerService() {}
std::string PowerService::execCommand(const std::vector<std::string> &args,
                                      int timeoutSec) {
  std::string result;
  executeCommandWithTimeout(args, result, timeoutSec);
  return result;
}

bool PowerService::isProcessAlive(const std::string &processName) {
  std::vector<std::string> args = {"pgrep", "-f", processName};
  std::string result;
  executeCommandWithTimeout(args, result, 2);
  return !result.empty();
}

bool PowerService::ensureAdbConnected(const std::string &address,
                                      int maxAttempts) {
  std::vector<std::string> startArgs = {"adb", "start-server"};
  executeCommandNoOutput(startArgs);
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    std::vector<std::string> stateArgs = {"adb", "get-state"};
    std::string result;
    executeCommandWithTimeout(stateArgs, result, 2);
    if (result.find("device") != std::string::npos)
      return true;
    std::vector<std::string> connectArgs = {"adb", "connect", address};
    std::string connectResult;
    executeCommandWithTimeout(connectArgs, connectResult, 3);
    if (connectResult.find("connected") != std::string::npos ||
        connectResult.find("already connected") != std::string::npos) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      executeCommandWithTimeout(stateArgs, result, 2);
      if (result.find("device") != std::string::npos)
        return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  return false;
}

bool PowerService::getTVScreenState() {
  std::vector<std::string> args = {"adb", "shell", "dumpsys", "power"};
  std::string result;
  executeCommandWithTimeout(args, result, 5);
  return result.find("mWakefulness=Awake") != std::string::npos ||
         result.find("Display Power: state=ON") != std::string::npos;
}

Json::Value PowerService::adbKillServer() {
  Json::Value result;
  std::vector<std::string> args = {"adb", "kill-server"};
  executeCommandNoOutput(args);
  result["success"] = true;
  result["message"] = "ADB server killed";
  return result;
}

Json::Value PowerService::adbStartServer() {
  Json::Value result;
  std::vector<std::string> args = {"adb", "start-server"};
  executeCommandNoOutput(args);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  result["success"] = true;
  result["message"] = "ADB server started";
  return result;
}

Json::Value PowerService::adbConnect(const std::string &address) {
  Json::Value result;
  std::vector<std::string> args = {"adb", "connect", address};
  std::string output;
  executeCommandWithTimeout(args, output, 5);
  bool success = output.find("connected") != std::string::npos ||
                 output.find("already connected") != std::string::npos;
  result["success"] = success;
  result["message"] = success ? "Connected to TV" : "Connection failed";
  result["address"] = address;
  result["output"] = output;
  return result;
}

Json::Value PowerService::adbKeyEvent(int keycode) {
  Json::Value result;
  std::vector<std::string> args = {"adb", "shell", "input", "keyevent",
                                   std::to_string(keycode)};
  std::string output;
  executeCommandWithTimeout(args, output, 5);
  bool success = output.empty() || output.find("error") == std::string::npos;
  result["success"] = success;
  result["message"] = success ? "Key event sent" : "Failed to send key event";
  result["keycode"] = keycode;
  return result;
}

Json::Value PowerService::adbGetState() {
  Json::Value result;
  std::vector<std::string> args = {"adb", "get-state"};
  std::string output;
  executeCommandWithTimeout(args, output, 5);
  bool connected = output.find("device") != std::string::npos;
  result["success"] = true;
  result["state"] = output;
  result["connected"] = connected;
  return result;
}

Json::Value PowerService::systemSleep() {
  Json::Value result;
  if (!m_initialized) {
    result["success"] = false;
    result["message"] = "Sleep blocked - sound system not available";
    return result;
  }
  std::lock_guard<std::mutex> lock(m_sleepMutex);
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - m_lastSleepCall)
          .count();
  if (m_isGoingToSleep || elapsed < 10) {
    result["success"] = false;
    result["message"] = "Sleep request ignored - too frequent";
    return result;
  }
  m_isGoingToSleep = true;
  m_lastSleepCall = now;
  std::vector<std::string> args = {"/usr/bin/systemctl", "suspend"};
  bool success = executeCommandNoOutput(args);
  result["success"] = success;
  result["message"] = success ? "System going to sleep" : "Failed to sleep";
  m_isGoingToSleep = false;
  return result;
}

Json::Value PowerService::getPowerStatus() {
  Json::Value result;
  if (!m_initialized) {
    result["success"] = true;
    result["tv_connected"] = false;
    result["tv_address"] = DEFAULT_TV_ADDRESS;
    result["media_player_running"] = false;
    result["warning"] = "Audio system not initialized";
    return result;
  }
  std::vector<std::string> startArgs = {"adb", "start-server"};
  executeCommandNoOutput(startArgs);
  std::vector<std::string> stateArgs = {"adb", "get-state"};
  std::string stateResult;
  executeCommandWithTimeout(stateArgs, stateResult, 2);
  bool tvConnected = stateResult.find("device") != std::string::npos;
  result["success"] = true;
  result["tv_connected"] = tvConnected;
  result["tv_address"] = DEFAULT_TV_ADDRESS;
  result["media_player_running"] = isProcessAlive("mpv.*--input-ipc-server");
  return result;
}

Json::Value PowerService::getTVPowerState() {
  Json::Value result;
  result["tv_address"] = DEFAULT_TV_ADDRESS;
  if (!m_initialized) {
    result["connected"] = false;
    result["state"] = "unknown";
    result["screen_on"] = false;
    result["wakefulness"] = "not_initialized";
    result["error"] = "Audio system not initialized";
    return result;
  }
  std::vector<std::string> startArgs = {"adb", "start-server"};
  executeCommandNoOutput(startArgs);
  std::vector<std::string> stateArgs = {"adb", "get-state"};
  std::string stateResult;
  executeCommandWithTimeout(stateArgs, stateResult, 3);
  bool connected = stateResult.find("device") != std::string::npos;
  result["connected"] = connected;
  result["state"] = stateResult.empty() ? "unknown" : stateResult;
  if (!connected) {
    result["screen_on"] = false;
    result["wakefulness"] = "disconnected";
    result["error"] = "ADB not connected to TV";
    return result;
  }
  std::vector<std::string> powerArgs = {"adb", "shell", "dumpsys", "power"};
  std::string powerResult;
  executeCommandWithTimeout(powerArgs, powerResult, 5);
  bool screenOn = false;
  std::string wakefulness = "Unknown";
  if (powerResult.find("mWakefulness=Awake") != std::string::npos) {
    wakefulness = "Awake";
    screenOn = true;
  } else if (powerResult.find("mWakefulness=Asleep") != std::string::npos) {
    wakefulness = "Asleep";
    screenOn = false;
  } else if (powerResult.find("mWakefulness=Dozing") != std::string::npos) {
    wakefulness = "Dozing";
    screenOn = false;
  } else if (powerResult.find("Display Power: state=ON") != std::string::npos) {
    screenOn = true;
  } else if (powerResult.find("Display Power: state=OFF") !=
             std::string::npos) {
    screenOn = false;
  }
  result["screen_on"] = screenOn;
  result["wakefulness"] = wakefulness;
  result["raw"] = powerResult.empty() ? "No data received" : powerResult;
  return result;
}

Json::Value PowerService::tvPowerOn() {
  Json::Value result;
  if (!m_initialized) {
    result["success"] = false;
    result["error"] = "Audio system not initialized";
    return result;
  }
  if (!ensureAdbConnected(DEFAULT_TV_ADDRESS, 3)) {
    result["success"] = false;
    result["error"] = "Failed to connect to TV via ADB";
    return result;
  }
  Json::Value keyResult = adbKeyEvent(26);
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  bool screenOn = getTVScreenState();
  result["success"] = screenOn;
  result["screen_on"] = screenOn;
  result["keycode_sent"] = keyResult["success"].asBool();
  result["message"] = screenOn ? "TV powered on successfully"
                               : "Key sent but TV did not turn on";
  return result;
}
