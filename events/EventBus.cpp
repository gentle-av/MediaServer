#include "EventBus.h"

void EventBus::subscribe(const std::string &event, EventCallback callback) {
  std::lock_guard<std::mutex> lock(mutex);
  subscribers[event].push_back(callback);
}

void EventBus::publish(const std::string &event, const std::string &data) {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = subscribers.find(event);
  if (it != subscribers.end()) {
    for (const auto &callback : it->second) {
      callback(data);
    }
  }
}
