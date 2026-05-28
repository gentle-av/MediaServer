#include "MonitorInfo.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <wayland-client.h>

void MonitorInfo::handle_global(void *data, struct wl_registry *registry,
                                uint32_t name, const char *interface,
                                uint32_t version) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (strcmp(interface, "wl_output") == 0) {
    struct wl_output *output = static_cast<wl_output *>(
        wl_registry_bind(registry, name, &wl_output_interface, 4));
    Monitor info;
    info.name = "output_" + std::to_string(name);
    info.isPrimary = self->monitors.empty();
    self->monitors.push_back(info);
    static const struct wl_output_listener output_listener = {
        handle_geometry, handle_mode, handle_done,
        handle_scale,    handle_name, handle_description};
    wl_output_add_listener(output, &output_listener, self);
  }
}

void MonitorInfo::handle_global_remove(void *data, struct wl_registry *registry,
                                       uint32_t name) {}

void MonitorInfo::handle_geometry(void *data, struct wl_output *output,
                                  int32_t x, int32_t y, int32_t physical_width,
                                  int32_t physical_height, int32_t subpixel,
                                  const char *make, const char *model,
                                  int32_t transform) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (!self->monitors.empty()) {
    auto &last = self->monitors.back();
    last.x = x;
    last.y = y;
    last.physical_width = physical_width;
    last.physical_height = physical_height;
    last.description = std::string(make) + " " + std::string(model);
  }
}

void MonitorInfo::handle_mode(void *data, struct wl_output *output,
                              uint32_t flags, int32_t width, int32_t height,
                              int32_t refresh) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (!self->monitors.empty()) {
    auto &last = self->monitors.back();
    if (flags & 0x1) {
      last.width = width;
      last.height = height;
    }
  }
}

void MonitorInfo::handle_done(void *data, struct wl_output *output) {}

void MonitorInfo::handle_scale(void *data, struct wl_output *output,
                               int32_t factor) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (!self->monitors.empty()) {
    self->monitors.back().scale = factor;
  }
}

void MonitorInfo::handle_name(void *data, struct wl_output *output,
                              const char *name) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (!self->monitors.empty()) {
    self->monitors.back().name = name;
  }
}

void MonitorInfo::handle_description(void *data, struct wl_output *output,
                                     const char *description) {
  auto *self = static_cast<MonitorInfo *>(data);
  if (!self->monitors.empty()) {
    self->monitors.back().description = description;
  }
}

std::vector<Monitor> MonitorInfo::getMonitors() {
  display = wl_display_connect(nullptr);
  if (!display) {
    std::cerr << "Не удалось подключиться к Wayland compositor" << std::endl;
    return {};
  }
  registry = wl_display_get_registry(display);
  static const struct wl_registry_listener registry_listener = {
      handle_global, handle_global_remove};
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(display);
  wl_display_roundtrip(display);
  return monitors;
}

MonitorInfo::~MonitorInfo() {
  if (registry)
    wl_registry_destroy(registry);
  if (display)
    wl_display_disconnect(display);
}
