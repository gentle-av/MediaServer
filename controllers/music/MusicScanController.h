#pragma once

#include "../../repositories/MusicRepository.h"
#include <html-server/app/App.h>
#include <html-server/controllers/HtmlController.h>

class MusicScanController : public HtmlController<App> {
private:
  MusicRepository &musicRepository;
  std::string musicDirectory;

public:
  explicit MusicScanController(App &app, MusicRepository &repo,
                               const std::string &musicDir);
  ~MusicScanController() = default;

protected:
  void register_all_routes() override;
};
