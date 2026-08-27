#include "DirectoryWatcher.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>

DirectoryWatcher::DirectoryWatcher(
    const std::string &path,
    std::function<void(const std::filesystem::path &)> callback)
    : path(path), running(true), callback(std::move(callback)) {
  watcher = std::thread(&DirectoryWatcher::watchLoop, this);
}

DirectoryWatcher::~DirectoryWatcher() { stop(); }

DirectoryWatcher::DirectoryWatcher(DirectoryWatcher &&other) noexcept
    : path(std::move(other.path)), running(other.running.load()),
      watcher(std::move(other.watcher)), callback(std::move(other.callback)) {
  other.running = false;
}

DirectoryWatcher &
DirectoryWatcher::operator=(DirectoryWatcher &&other) noexcept {
  if (this != &other) {
    stop();
    path = std::move(other.path);
    running = other.running.load();
    watcher = std::move(other.watcher);
    callback = std::move(other.callback);
    other.running = false;
  }
  return *this;
}

void DirectoryWatcher::setCallback(
    std::function<void(const std::filesystem::path &)> callback) {
  this->callback = std::move(callback);
}

void DirectoryWatcher::stop() {
  running = false;
  if (watcher.joinable()) {
    watcher.join();
  }
}

void DirectoryWatcher::watchLoop() {
  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    std::cerr << "Failed to initialize inotify: " << strerror(errno)
              << std::endl;
    return;
  }
  int wd = inotify_add_watch(fd, path.c_str(),
                             IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE);
  if (wd < 0) {
    std::cerr << "Failed to add watch for directory: " << strerror(errno)
              << std::endl;
    close(fd);
    return;
  }
  char buffer[4096];
  while (running) {
    int length = read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      if (errno == EAGAIN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      break;
    }
    int i = 0;
    while (i < length) {
      auto *event = reinterpret_cast<struct inotify_event *>(&buffer[i]);
      if (event->mask & (IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE)) {
        if (!(event->mask & IN_ISDIR) && event->len > 0) {
          std::string filename(event->name);
          onFileAdded(filename);
        }
      }
      i += sizeof(struct inotify_event) + event->len;
    }
  }
  inotify_rm_watch(fd, wd);
  close(fd);
}

void DirectoryWatcher::onFileAdded(const std::string &filename) {
  std::filesystem::path filePath = std::filesystem::path(path) / filename;
  if (!std::filesystem::exists(filePath)) {
    return;
  }
  static const std::vector<std::string> videoExtensions = {
      ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", ".m4v"};
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  bool isVideo = std::any_of(
      videoExtensions.begin(), videoExtensions.end(),
      [&ext](const std::string &validExt) { return ext == validExt; });
  if (!isVideo) {
    return;
  }
  if (callback) {
    callback(filePath);
  }
}
