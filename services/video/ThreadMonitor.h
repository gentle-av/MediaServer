#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>

class ThreadMonitor {
public:
  static ThreadMonitor &getInstance();
  void registerThread(const std::string &name, std::thread::id id);
  void unregisterThread(std::thread::id id);
  void startWait(std::thread::id id, const std::string &location);
  void endWait(std::thread::id id);
  std::string getWaitReport() const;
  std::map<std::thread::id, std::string> getWaitingThreads() const;

private:
  ThreadMonitor() = default;

  struct ThreadInfo {
    std::string name;
    std::string waitingSince;
    std::string waitLocation;
  };

  mutable std::mutex mtx;
  std::map<std::thread::id, ThreadInfo> threads;
};
