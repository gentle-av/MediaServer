cmake_minimum_required(VERSION 3.12)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install path" FORCE)

find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(HtmlServer REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

set(COMMON_LIBS
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
add_library(MetadataExtractor SHARED services/music/MetadataExtractor.cpp services/music/MetadataCache.cpp)
add_library(MusicDatabase SHARED database/MusicDatabase.cpp)
add_library(MkvAudioTrackInfo SHARED models/MkvAudioTrackInfo.cpp)
add_library(MusicRepository SHARED repositories/MusicRepository.cpp)
add_library(Playlist SHARED models/Playlist.cpp)
add_library(PlaylistDatabase SHARED database/PlaylistDatabase.cpp)
add_library(PlaylistRepository SHARED repositories/PlaylistRepository.cpp)
add_library(FileSystemService SHARED services/video/FileSystemService.cpp services/video/TrashHandler.cpp)
add_library(PlaybackService SHARED services/video/PlaybackService.cpp services/video/PlaybackStatus.cpp services/player/AutoAdvanceTracker.cpp)
add_library(AlsaMixer SHARED services/system/AlsaMixer.cpp)
add_library(VideoControlHandler SHARED services/video/VideoControlHandler.cpp)
add_library(PlaylistManager SHARED services/music/PlaylistManager.cpp)
add_library(Profiler SHARED profilers/Profiler.cpp)
add_library(MusicScanController SHARED controllers/music/MusicScanController.cpp)
add_library(MusicLibraryController SHARED controllers/music/MusicLibraryController.cpp)
add_library(PlaylistController SHARED controllers/playlists/PlaylistController.cpp)
add_library(PlayerController SHARED controllers/player/PlayerController.cpp services/player/AudioOutputSwitcher.cpp)
add_library(VideoController SHARED controllers/video/VideoController.cpp services/video/StaticFileService.cpp)
add_library(PowerService SHARED services/system/PowerService.cpp services/system/MonitorService.cpp)
add_library(PowerController SHARED controllers/power/PowerController.cpp)
add_library(MediaServerCore SHARED core/MediaServerCore.cpp)

target_link_libraries(MusicRepository
    ${COMMON_LIBS}
    MkvAudioTrackInfo
    MusicDatabase
    TagEditor
    MetadataExtractor
    EventBus
)

target_link_libraries(PlaylistDatabase Playlist)
target_link_libraries(PlaylistRepository Playlist MusicRepository EventBus ${COMMON_LIBS})
target_link_libraries(FileSystemService ${COMMON_LIBS})
target_link_libraries(PlaybackService FileSystemService ${COMMON_LIBS})
target_link_libraries(AlsaMixer ${COMMON_LIBS})
target_link_libraries(VideoControlHandler PlaybackService FileSystemService AlsaMixer ${COMMON_LIBS})

target_link_libraries(PlaylistManager
    Playlist
    PlaylistRepository
    MusicRepository
    VideoControlHandler
    PlaybackService
    FileSystemService
    AlsaMixer
    ${COMMON_LIBS}
)

target_link_libraries(Profiler ${COMMON_LIBS})
target_link_libraries(MusicScanController MusicRepository PlaylistRepository ${COMMON_LIBS})
target_link_libraries(MusicLibraryController MusicRepository ${COMMON_LIBS})
target_link_libraries(PlaylistController PlaylistRepository MusicRepository ${COMMON_LIBS})
target_link_libraries(PlayerController ${COMMON_LIBS})
target_link_libraries(VideoController FileSystemService PlaybackService VideoControlHandler ${COMMON_LIBS})
target_link_libraries(PowerService ${COMMON_LIBS})
target_link_libraries(PowerController ${COMMON_LIBS} PowerService)

target_link_libraries(MediaServerCore
    PRIVATE
    MusicLibraryController
    MusicScanController
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
    EventBus
    TagEditor
    MkvAudioTrackInfo
    PlayerController
    VideoController
    ${COMMON_LIBS}
)

target_compile_definitions(MusicRepository PRIVATE TAGLIB_STATIC)

set_target_properties(MusicRepository PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

set_target_properties(Profiler PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

add_executable(MediaServer main.cpp)
target_link_libraries(MediaServer PRIVATE MediaServerCore)

install(TARGETS MediaServer RUNTIME DESTINATION bin/)

install(TARGETS
    EventBus
    TagEditor
    MetadataExtractor
    MusicDatabase
    MkvAudioTrackInfo
    MusicRepository
    Playlist
    PlaylistDatabase
    PlaylistRepository
    FileSystemService
    PlaybackService
    AlsaMixer
    VideoControlHandler
    PlaylistManager
    Profiler
    MusicScanController
    MusicLibraryController
    PlaylistController
    PlayerController
    VideoController
    PowerService
    PowerController
    MediaServerCore
    LIBRARY DESTINATION lib/media-explorer/
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/controllers/
    DESTINATION include/media-explorer/controllers
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/core/
    DESTINATION include/media-explorer/core
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/database/
    DESTINATION include/media-explorer/database
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/events/
    DESTINATION include/media-explorer/events
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/middlewares/
    DESTINATION include/media-explorer/middlewares
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/models/
    DESTINATION include/media-explorer/models
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/profilers/
    DESTINATION include/media-explorer/profilers
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/repositories/
    DESTINATION include/media-explorer/repositories
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/services/
    DESTINATION include/media-explorer/services
    FILES_MATCHING
    PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tagger/
    DESTINATION include/media-explorer/tagger
    FILES_MATCHING
    PATTERN "*.h"
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    COPYONLY
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
    DESTINATION share/media-explorer/
)

add_custom_command(
    TARGET MediaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/media-explorer/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
        $ENV{HOME}/.local/share/media-explorer/config.json
    COMMENT "Copying config.json to user config directory"
)

add_custom_command(
    TARGET MediaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
    COMMENT "Copying config.json to build directory"
)

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Executable will be installed to: ${CMAKE_INSTALL_PREFIX}/bin/")
message(STATUS "Libraries will be installed to: ${CMAKE_INSTALL_PREFIX}/lib/media-explorer/")
message(STATUS "Headers will be installed to: ${CMAKE_INSTALL_PREFIX}/include/media-explorer/")
message(STATUS "Config will be installed to: ${CMAKE_INSTALL_PREFIX}/share/media-explorer/")
