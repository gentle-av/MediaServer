#pragma once

#include "services/player/AudioOutputService.h"
#include "services/player/AudioPlaybackService.h"
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <memory>

class PlayerController : public RestController<App> {
public:
  explicit PlayerController(
      App &app, std::shared_ptr<AudioPlaybackService> playbackService,
      std::shared_ptr<AudioOutputService> outputService);

protected:
  void register_all_routes() override;

private:
  std::shared_ptr<AudioPlaybackService> playbackService;
  std::shared_ptr<AudioOutputService> outputService;
};
