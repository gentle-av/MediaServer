#pragma once

#include <json/json.h>
#include <string>

class TrashHandler {
public:
  static TrashHandler &getInstance();
  Json::Value handleMoveToTrash(const std::string &path);
  Json::Value handleDeleteDirectory(const std::string &path);

private:
  TrashHandler() = default;
};
