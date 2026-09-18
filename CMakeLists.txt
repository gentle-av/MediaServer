cmake_minimum_required(VERSION 3.12)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install path" FORCE)

set(HTML_SERVER_ROOT "$ENV{HOME}/code/projects/cpp/HtmlServer")

find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_subdirectory(
    ${HTML_SERVER_ROOT}
    ${CMAKE_BINARY_DIR}/HtmlServer
)

add_library(HtmlServer::AppCore ALIAS AppCore)
add_library(HtmlServer::AuthCore ALIAS AuthCore)
add_library(HtmlServer::FileServerCore ALIAS FileServerCore)
add_library(HtmlServer::JsonCore ALIAS JsonCore)
add_library(HtmlServer::Core ALIAS Core)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${HTML_SERVER_ROOT}/include
    ${HTML_SERVER_ROOT}/include/app
    ${HTML_SERVER_ROOT}/include/auth
    ${HTML_SERVER_ROOT}/include/concepts
    ${HTML_SERVER_ROOT}/include/controllers
    ${HTML_SERVER_ROOT}/include/core
    ${HTML_SERVER_ROOT}/include/fs
    ${HTML_SERVER_ROOT}/include/interfaces
    ${HTML_SERVER_ROOT}/include/serializer
    ${HTML_SERVER_ROOT}/include/session
    ${HTML_SERVER_ROOT}/include/templates
    ${HTML_SERVER_ROOT}/include/web
)

set(commonLibs
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${DBUS_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    HtmlServer::AppCore
    HtmlServer::AuthCore
    HtmlServer::FileServerCore
    HtmlServer::JsonCore
    HtmlServer::Core
    asound
    pthread
    dl
)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

add_library(EventBus SHARED events/EventBus.cpp)
add_library(TagEditor SHARED tagger/TagEditor.cpp)
add_library(MusicDatabase SHARED database/MusicDatabase.cpp)
add_library(MkvAudioTrackInfo SHARED models/MkvAudioTrackInfo.cpp)
add_library(Playlist SHARED models/Playlist.cpp)
add_library(PlaylistDatabase SHARED database/PlaylistDatabase.cpp)
add_library(Profiler SHARED profilers/Profiler.cpp)
add_library(AlsaMixer SHARED services/system/AlsaMixer.cpp)

add_library(MetadataExtractor SHARED
    services/music/MetadataExtractor.cpp
    services/music/MetadataCache.cpp
)
add_library(MpvIpcClient SHARED services/player/MpvIpcClient.cpp)
add_library(AudioPlaybackService SHARED services/player/AudioPlaybackService.cpp)
add_library(AudioOutputService SHARED services/player/AudioOutputService.cpp)

add_library(MusicRepository SHARED repositories/MusicRepository.cpp)
add_library(PlaylistRepository SHARED repositories/PlaylistRepository.cpp)
add_library(FileSystemService SHARED services/video/FileSystemService.cpp services/video/TrashHandler.cpp)
add_library(PlaybackService SHARED services/video/PlaybackService.cpp services/video/PlaybackStatus.cpp services/player/AutoAdvanceTracker.cpp)
add_library(VideoControlHandler SHARED services/video/VideoControlHandler.cpp)
add_library(PlaylistManager SHARED services/music/PlaylistManager.cpp)
add_library(PowerService SHARED services/system/PowerService.cpp services/system/MonitorService.cpp)

add_library(MusicScanController SHARED controllers/music/MusicScanController.cpp)
add_library(MusicLibraryController SHARED controllers/music/MusicLibraryController.cpp)
add_library(MusicMetadataController SHARED controllers/music/MusicMetadataController.cpp)
add_library(MusicPlaybackController SHARED controllers/music/MusicPlaybackController.cpp)
add_library(PlaylistController SHARED controllers/playlists/PlaylistController.cpp)
add_library(PlayerController SHARED controllers/player/PlayerController.cpp services/player/AudioOutputSwitcher.cpp)
add_library(VideoController SHARED controllers/video/VideoController.cpp services/video/StaticFileService.cpp)
add_library(PowerController SHARED controllers/power/PowerController.cpp)
add_library(AlbumArtController SHARED controllers/albums/AlbumArtController.cpp)
add_library(AlbumManagementController SHARED controllers/albums/AlbumManagementController.cpp)
add_library(MonitorController SHARED controllers/system/MonitorController.cpp)
add_library(MediaServerCore SHARED core/MediaServerCore.cpp)

target_link_libraries(MusicRepository
    ${commonLibs}
    MkvAudioTrackInfo
    MusicDatabase
    TagEditor
    MetadataExtractor
    EventBus
)

target_link_libraries(MpvIpcClient ${commonLibs})
target_link_libraries(AudioPlaybackService MpvIpcClient ${commonLibs})
target_link_libraries(AudioOutputService AlsaMixer ${commonLibs})

target_link_libraries(PlaylistDatabase Playlist)
target_link_libraries(PlaylistRepository Playlist MusicRepository EventBus ${commonLibs})
target_link_libraries(FileSystemService ${commonLibs})
target_link_libraries(PlaybackService FileSystemService ${commonLibs})
target_link_libraries(VideoControlHandler PlaybackService FileSystemService AlsaMixer ${commonLibs})

target_link_libraries(PlaylistManager
    Playlist
    PlaylistRepository
    MusicRepository
    VideoControlHandler
    PlaybackService
    FileSystemService
    AlsaMixer
    ${commonLibs}
)

target_link_libraries(Profiler ${commonLibs})
target_link_libraries(MusicScanController MusicRepository PlaylistRepository ${commonLibs})
target_link_libraries(MusicLibraryController MusicRepository ${commonLibs})
target_link_libraries(MusicMetadataController MusicDatabase MetadataExtractor ${commonLibs})
target_link_libraries(MusicPlaybackController MusicDatabase PlayerController ${commonLibs})
target_link_libraries(PlaylistController PlaylistRepository MusicRepository ${commonLibs})
target_link_libraries(PlayerController AudioPlaybackService AudioOutputService ${commonLibs} AlsaMixer)
target_link_libraries(VideoController FileSystemService PlaybackService VideoControlHandler ${commonLibs})
target_link_libraries(PowerService ${commonLibs})
target_link_libraries(PowerController ${commonLibs} PowerService)
target_link_libraries(AlbumArtController MusicRepository MusicDatabase ${commonLibs})
target_link_libraries(AlbumManagementController MusicRepository MusicDatabase FileSystemService ${commonLibs})
target_link_libraries(MonitorController PowerService ${commonLibs})

target_link_libraries(MediaServerCore
    PRIVATE
    MusicLibraryController
    MusicScanController
    MusicMetadataController
    MusicPlaybackController
    PlaylistController
    PowerController
    PowerService
    MusicRepository
    Profiler
    Playlist
    PlaylistDatabase
    PlaylistRepository
    PlaylistManager
    VideoControlHandler
    PlaybackService
    FileSystemService
    AlsaMixer
    MetadataExtractor
    MpvIpcClient
    AudioPlaybackService
    AudioOutputService
    EventBus
    TagEditor
    MkvAudioTrackInfo
    PlayerController
    VideoController
    AlbumArtController
    AlbumManagementController
    MonitorController
    ${commonLibs}
)

target_compile_definitions(MusicRepository PRIVATE TAGLIB_STATIC)
set_target_properties(MusicRepository PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
set_target_properties(Profiler PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

add_executable(MediaServer main.cpp)
target_link_libraries(MediaServer PRIVATE MediaServerCore)

install(TARGETS MediaServer RUNTIME DESTINATION bin/)
install(TARGETS
    EventBus TagEditor MetadataExtractor MusicDatabase MkvAudioTrackInfo
    MusicRepository Playlist PlaylistDatabase PlaylistRepository FileSystemService
    PlaybackService AlsaMixer VideoControlHandler PlaylistManager Profiler
    MpvIpcClient AudioPlaybackService AudioOutputService
    MusicScanController MusicLibraryController MusicMetadataController MusicPlaybackController
    PlaylistController PlayerController VideoController PowerService PowerController
    AlbumArtController AlbumManagementController MonitorController MediaServerCore
    LIBRARY DESTINATION lib/media-explorer/
)

set(headersDirs controllers core database events middlewares models profilers repositories services tagger)
foreach(dir ${headersDirs})
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/
        DESTINATION include/media-explorer/${dir}
        FILES_MATCHING PATTERN "*.h"
    )
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json DESTINATION share/media-explorer/)

add_custom_command(
    TARGET MediaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/media-explorer/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
        $ENV{HOME}/.local/share/media-explorer/config.json
)

add_custom_command(
    TARGET MediaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
)
