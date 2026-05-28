#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Monitor {
  std::string name;
  std::string description;
  int32_t x, y;
  int32_t width, height;
  int32_t physical_width;
  int32_t physical_height;
  int32_t scale;
  bool isPrimary;
};

class MonitorInfo {
private:
  struct wl_display *display = nullptr;
  struct wl_registry *registry = nullptr;
  std::vector<Monitor> monitors;
  bool done = false;

  static void handle_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version);
  static void handle_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name);
  static void handle_geometry(void *data, struct wl_output *output, int32_t x,
                              int32_t y, int32_t physical_width,
                              int32_t physical_height, int32_t subpixel,
                              const char *make, const char *model,
                              int32_t transform);
  static void handle_mode(void *data, struct wl_output *output, uint32_t flags,
                          int32_t width, int32_t height, int32_t refresh);
  static void handle_done(void *data, struct wl_output *output);
  static void handle_scale(void *data, struct wl_output *output,
                           int32_t factor);
  static void handle_name(void *data, struct wl_output *output,
                          const char *name);
  static void handle_description(void *data, struct wl_output *output,
                                 const char *description);

public:
  std::vector<Monitor> getMonitors();
  ~MonitorInfo();
};
