cmake_minimum_required(VERSION 3.12)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(HtmlServer REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

set(COMMON_LIBS
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${DBUS_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    HtmlServer::AppCore
    HtmlServer::AuthCore
    HtmlServer::FileServerCore
    HtmlServer::JsonCore
    HtmlServer::Interfaces
    HtmlServer::Concepts
    asound
    pthread
    dl
)

set(COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

add_library(media_tagger SHARED
    tagger/TagEditor.cpp
)

add_library(media_metadata_extractor SHARED
    services/music/MetadataExtractor.cpp
    services/music/MetadataCache.cpp
)

add_library(media_music_database SHARED
    database/MusicDatabase.cpp
)

add_library(media_trackinfo SHARED
    models/MkvAudioTrackInfo.cpp
)

add_library(media_repository SHARED
    repositories/MusicRepository.cpp
)

target_include_directories(media_repository PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_repository
    ${COMMON_LIBS}
    media_trackinfo
    media_music_database
    media_tagger
    media_metadata_extractor
)
target_compile_definitions(media_repository PRIVATE TAGLIB_STATIC)
set_target_properties(media_repository PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

add_library(media_playlist SHARED
    models/Playlist.cpp
)

add_library(media_playlist_database SHARED
    database/PlaylistDatabase.cpp
)

target_link_libraries(media_playlist_database PRIVATE
    media_playlist
)

add_library(media_playlist_repository SHARED
    repositories/PlaylistRepository.cpp
)

target_link_libraries(media_playlist_repository PRIVATE
    media_playlist
    media_repository
)

add_library(media_filesystem SHARED
    services/video/FileSystemService.cpp
    services/video/TrashHandler.cpp
)

target_include_directories(media_filesystem PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_filesystem ${COMMON_LIBS})

add_library(media_playback SHARED
    services/video/PlaybackService.cpp
    services/video/PlaybackStatus.cpp
    services/player/AutoAdvanceTracker.cpp
)

target_include_directories(media_playback PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_playback
    media_filesystem
    ${COMMON_LIBS}
)

add_library(media_alsa SHARED
    services/system/AlsaMixer.cpp
)

target_include_directories(media_alsa PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_alsa ${COMMON_LIBS})

add_library(media_video_control SHARED
    services/video/VideoControlHandler.cpp
)

target_include_directories(media_video_control PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_video_control
    media_playback
    media_filesystem
    media_alsa
    ${COMMON_LIBS}
)

add_library(media_playlist_manager SHARED
    services/music/PlaylistManager.cpp
)

target_include_directories(media_playlist_manager PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_playlist_manager PRIVATE
    media_playlist
    media_playlist_repository
    media_repository
    media_video_control
    media_playback
    media_filesystem
    media_alsa
    ${COMMON_LIBS}
)

add_library(media_profiler SHARED
    profilers/Profiler.cpp
)

target_include_directories(media_profiler PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_profiler ${COMMON_LIBS})
set_target_properties(media_profiler PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

add_library(media_video_thumbnailer SHARED
    services/video/VideoThumbnailer.cpp
)

target_include_directories(media_video_thumbnailer PRIVATE
    ${COMMON_INCLUDES}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
)
target_link_libraries(media_video_thumbnailer PRIVATE
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${COMMON_LIBS}
)

add_library(media_music_scan_controller SHARED
    controllers/music/MusicScanController.cpp
)

target_include_directories(media_music_scan_controller PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_music_scan_controller
    media_repository
    ${COMMON_LIBS}
)

add_library(media_music_library_controller SHARED
    controllers/music/MusicLibraryController.cpp
)

target_include_directories(media_music_library_controller PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_music_library_controller
    media_repository
    ${COMMON_LIBS}
)

add_executable(MediaServer ./main.cpp)

target_include_directories(MediaServer PRIVATE ${COMMON_INCLUDES})

target_link_libraries(MediaServer
    PRIVATE
    media_music_library_controller
    media_music_scan_controller
    media_repository
    media_profiler
    media_playlist
    media_playlist_database
    media_playlist_repository
    media_playlist_manager
    media_video_control
    media_playback
    media_filesystem
    media_alsa
    media_video_thumbnailer
    media_metadata_extractor
    ${COMMON_LIBS}
)
