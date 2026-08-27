#pragma once

#include "../../repositories/MusicRepository.h"
#include "../../repositories/PlaylistRepository.h"
#include "../../services/music/MetadataCache.h"
#include "../../services/player/AudioOutputSwitcher.h"
#include "../../services/player/AutoAdvanceTracker.h"
#include <atomic>
#include <html-server/app/App.h>
#include <html-server/controllers/RestController.h>
#include <html-server/templates/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>

class PlayerController : public RestController<App> {
public:
  explicit PlayerController(App &app, MusicRepository &musicRepo,
                            PlaylistRepository &playlistRepo,
                            std::shared_ptr<MetadataCache> cache = nullptr);
  ~PlayerController() override;

protected:
  void register_all_routes() override;

private:
  MusicRepository &musicRepository;
  PlaylistRepository &playlistRepository;
  std::shared_ptr<MetadataCache> metadataCache;
  std::unique_ptr<AudioOutputSwitcher> outputSwitcher;
  std::unique_ptr<AutoAdvanceTracker> autoAdvanceTracker;
  std::string socketPath;
  std::string currentPlaylistName;
  std::vector<std::string> currentPlaylistTracks;
  std::atomic<int> currentIndex{-1};
  std::atomic<bool> isPlaying{false};
  std::atomic<bool> stopAutoAdvance{false};
  std::unique_ptr<std::thread> idleTimerThread;
  std::mutex timerMutex;
  void *mpvHandle = nullptr;
  mutable std::shared_mutex stateMutex;

  StringHttpResponse handlePlay(const StringHttpRequest &req);
  StringHttpResponse handlePause(const StringHttpRequest &req);
  StringHttpResponse handleStop(const StringHttpRequest &req);
  StringHttpResponse handleNext(const StringHttpRequest &req);
  StringHttpResponse handlePrevious(const StringHttpRequest &req);
  StringHttpResponse handleSetPlaylist(const StringHttpRequest &req);
  StringHttpResponse handleLoadPlaylist(const StringHttpRequest &req);
  StringHttpResponse handlePlayFile(const StringHttpRequest &req);
  StringHttpResponse handlePlayIndex(const StringHttpRequest &req);
  StringHttpResponse handleSeek(const StringHttpRequest &req);
  StringHttpResponse handleSetSpeed(const StringHttpRequest &req);
  StringHttpResponse handleGetPlaybackState(const StringHttpRequest &req);
  StringHttpResponse handleGetCurrentTime(const StringHttpRequest &req);
  StringHttpResponse handleGetVolume(const StringHttpRequest &req);
  StringHttpResponse handleSetVolume(const StringHttpRequest &req);
  StringHttpResponse handleIncreaseVolume(const StringHttpRequest &req);
  StringHttpResponse handleDecreaseVolume(const StringHttpRequest &req);
  StringHttpResponse handleToggleMute(const StringHttpRequest &req);
  StringHttpResponse handleSwitchToSpeakers(const StringHttpRequest &req);
  StringHttpResponse handleSwitchToHeadphones(const StringHttpRequest &req);
  StringHttpResponse handleGetAudioOutput(const StringHttpRequest &req);
  StringHttpResponse handleForceStop(const StringHttpRequest &req);

  void startMpvIfNeeded();
  void loadTrack(int index);
  void loadPlaylistInternal(const std::string &name);
  void resetIdleTimer();
  bool ensurePlaylistLoaded();
  nlohmann::json getCurrentPlaybackState() const;
  nlohmann::json getCurrentTimeInfo() const;
  nlohmann::json trackToJson(const MusicMetadata &track) const;
  std::string getQueryParam(const StringHttpRequest &req,
                            const std::string &key,
                            const std::string &defaultValue = "") const;
  int getQueryParamInt(const StringHttpRequest &req, const std::string &key,
                       int defaultValue = 1) const;
  bool getQueryParamBool(const StringHttpRequest &req, const std::string &key,
                         bool defaultValue = false) const;
  nlohmann::json parseJsonBody(const StringHttpRequest &req) const;
  std::string formatTime(double seconds) const;
  std::string sendMpvCommand(const std::string &command) const;
  double parseMpvResponse(const std::string &response) const;
};
