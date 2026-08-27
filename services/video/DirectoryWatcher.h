#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

class DirectoryWatcher {
public:
  explicit DirectoryWatcher(
      const std::string &path,
      std::function<void(const std::filesystem::path &)> callback = nullptr);
  ~DirectoryWatcher();
  DirectoryWatcher(const DirectoryWatcher &) = delete;
  DirectoryWatcher &operator=(const DirectoryWatcher &) = delete;
  DirectoryWatcher(DirectoryWatcher &&) noexcept;
  DirectoryWatcher &operator=(DirectoryWatcher &&) noexcept;
  void setCallback(std::function<void(const std::filesystem::path &)> callback);
  void stop();
  bool isRunning() const { return running; }

private:
  void watchLoop();
  void onFileAdded(const std::string &filename);
  std::string path;
  std::atomic<bool> running;
  std::thread watcher;
  std::function<void(const std::filesystem::path &)> callback;
};
