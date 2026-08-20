#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class EventBus {
public:
  using EventCallback = std::function<void(const std::string &)>;

  void subscribe(const std::string &event, EventCallback callback);
  void publish(const std::string &event, const std::string &data = "");

private:
  std::unordered_map<std::string, std::vector<EventCallback>> subscribers;
  std::mutex mutex;
};
