#include "ManualThumbnailController.h"
#include <html-server/app/App.h>
#include <nlohmann/json.hpp>

ManualThumbnailController::ManualThumbnailController(App &app,
                                                     ImageDatabase &database)
    : RestController<App>(app), database(database), updater(database) {}

void ManualThumbnailController::register_all_routes() {
  app_.post("/api/thumbnail/update",
            [this](const StringHttpRequest &req) -> StringHttpResponse {
              return handleUpdateThumbnail(req);
            });
}

StringHttpResponse
ManualThumbnailController::handleUpdateThumbnail(const StringHttpRequest &req) {
  StringHttpResponse res;
  nlohmann::json responseJson;
  try {
    auto bodyJson = parseJsonBody(req);
    if (bodyJson.is_null() || !bodyJson.contains("videoPath") ||
        !bodyJson["videoPath"].is_string()) {
      responseJson["success"] = false;
      responseJson["error"] = "Missing or invalid videoPath parameter";
      res.setJsonContent(responseJson.dump());
      res.setStatus(400);
      return res;
    }
    std::string videoPath = bodyJson["videoPath"].get<std::string>();
    std::filesystem::path path(videoPath);
    bool success = false;
    if (bodyJson.contains("timestamp") && bodyJson["timestamp"].is_number()) {
      double timestamp = bodyJson["timestamp"].get<double>();
      success = updater.updateThumbnail(path, timestamp);
    } else {
      success = updater.updateThumbnail(path);
    }
    if (success) {
      responseJson["success"] = true;
      responseJson["message"] = "Thumbnail updated successfully";
      res.setJsonContent(responseJson.dump());
      res.setStatus(200);
    } else {
      responseJson["success"] = false;
      responseJson["error"] = "Failed to update thumbnail";
      res.setJsonContent(responseJson.dump());
      res.setStatus(500);
    }
  } catch (const nlohmann::json::parse_error &e) {
    responseJson["success"] = false;
    responseJson["error"] = "Invalid JSON payload";
    res.setJsonContent(responseJson.dump());
    res.setStatus(400);
  } catch (const std::exception &e) {
    responseJson["success"] = false;
    responseJson["error"] = std::string("Internal error: ") + e.what();
    res.setJsonContent(responseJson.dump());
    res.setStatus(500);
  }
  return res;
}

nlohmann::json
ManualThumbnailController::parseJsonBody(const StringHttpRequest &req) const {
  try {
    return nlohmann::json::parse(req.getBodyString());
  } catch (...) {
    return nlohmann::json();
  }
}
