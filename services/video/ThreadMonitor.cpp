#include "ThreadMonitor.h"
#include <iomanip>
#include <sstream>

ThreadMonitor &ThreadMonitor::getInstance() {
  static ThreadMonitor instance;
  return instance;
}

void ThreadMonitor::registerThread(const std::string &name,
                                   std::thread::id id) {
  std::lock_guard<std::mutex> lock(mtx);
  threads[id].name = name;
}

void ThreadMonitor::unregisterThread(std::thread::id id) {
  std::lock_guard<std::mutex> lock(mtx);
  threads.erase(id);
}

void ThreadMonitor::startWait(std::thread::id id, const std::string &location) {
  std::lock_guard<std::mutex> lock(mtx);
  auto it = threads.find(id);
  if (it != threads.end()) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    it->second.waitingSince = ss.str();
    it->second.waitLocation = location;
  }
}

void ThreadMonitor::endWait(std::thread::id id) {
  std::lock_guard<std::mutex> lock(mtx);
  auto it = threads.find(id);
  if (it != threads.end()) {
    it->second.waitingSince.clear();
    it->second.waitLocation.clear();
  }
}

std::string ThreadMonitor::getWaitReport() const {
  std::lock_guard<std::mutex> lock(mtx);
  std::stringstream report;
  report << "=== Thread Wait Report ===\n";
  for (const auto &pair : threads) {
    report << "Thread: " << pair.second.name << " (ID: " << pair.first << ")\n";
    if (!pair.second.waitingSince.empty()) {
      report << "  Waiting since: " << pair.second.waitingSince << "\n";
      report << "  Waiting at: " << pair.second.waitLocation << "\n";
    } else {
      report << "  Status: Not waiting\n";
    }
  }
  return report.str();
}

std::map<std::thread::id, std::string>
ThreadMonitor::getWaitingThreads() const {
  std::lock_guard<std::mutex> lock(mtx);
  std::map<std::thread::id, std::string> waiting;
  for (const auto &pair : threads) {
    if (!pair.second.waitingSince.empty()) {
      waiting[pair.first] = pair.second.name;
    }
  }
  return waiting;
}
