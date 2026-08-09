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
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)

set(COMMON_LIBS
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${DBUS_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
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
)

add_library(media_tagger SHARED
    tagger/TagEditor.cpp
)

add_library(media_metadata_extractor SHARED
    services/music/MetadataExtractor.cpp
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

# add_library(media_music SHARED
#     services/music/AlbumArtExtractor.cpp
#     services/music/AlbumArtService.cpp
#     services/music/AlbumArtWriter.cpp
#     services/music/MetadataCache.cpp
#     services/music/MetadataExtractor.cpp
#     services/music/MusicScanner.cpp
#     services/music/PlaylistManager.cpp
#     services/music/ResponseBuild.cpp
# )
#
# target_include_directories(media_music PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_music media_core ${COMMON_LIBS})
# target_compile_definitions(media_music PRIVATE TAGLIB_STATIC)
# set_target_properties(media_music PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
# add_library(media_player SHARED
#     services/player/AudioOutputSwitcher.cpp
#     services/player/AutoAdvanceTracker.cpp
#     services/player/MetadataParser.cpp
#     services/player/MkvAudioChannel.cpp
#     services/player/MpvCommandSender.cpp
#     services/player/PlayerSessionManager.cpp
#     services/player/TracklistManager.cpp
#     services/player/TrackLoader.cpp
#     services/player/Volumer.cpp
# )
#
# target_include_directories(media_player PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_player media_core ${COMMON_LIBS})
# target_compile_definitions(media_player PRIVATE TAGLIB_STATIC)
# set_target_properties(media_player PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
# add_library(media_system SHARED
#     services/system/AlsaMixer.cpp
#     services/system/MonitorService.cpp
#     services/system/PowerService.cpp
# )
#
# target_include_directories(media_system PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_system ${COMMON_LIBS})
# set_target_properties(media_system PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
# add_library(media_video SHARED
#     services/video/FileSystemService.cpp
#     services/video/PlaybackService.cpp
#     services/video/PlaybackStatus.cpp
#     services/video/StaticFileService.cpp
#     services/video/ThreadMonitor.cpp
#     services/video/TrashHandler.cpp
#     services/video/VideoControlHandler.cpp
# )
#
# target_include_directories(media_video PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_video media_core ${COMMON_LIBS})
# set_target_properties(media_video PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
# add_library(media_tagger SHARED
#     tagger/TagEditor.cpp
# )
#
# target_include_directories(media_tagger PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_tagger TagLib::TagLib)
# target_compile_definitions(media_tagger PRIVATE TAGLIB_STATIC)
# set_target_properties(media_tagger PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
add_library(media_profiler SHARED
    profilers/Profiler.cpp
)

target_include_directories(media_profiler PRIVATE ${COMMON_INCLUDES})
target_link_libraries(media_profiler ${COMMON_LIBS})
set_target_properties(media_profiler PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

add_executable(MediaServer ./main.cpp)
target_link_libraries(MediaServer
  PRIVATE
  media_repository
  media_profiler)
#
# add_library(media_controllers SHARED
#     controllers/music/MusicScanController.cpp
# )
#
# target_include_directories(media_controllers PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(media_controllers
#     media_core
#     media_music
#     ${COMMON_LIBS}
# )
# set_target_properties(media_controllers PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
# )
#
# add_executable(MediaServer main.cpp)
#
# target_include_directories(MediaServer PRIVATE ${COMMON_INCLUDES})
# target_link_libraries(MediaServer
#     media_core
#     media_music
#     media_player
#     media_system
#     media_video
#     media_tagger
#     media_profiler
#     media_controllers
#     ${COMMON_LIBS}
# )
#
# target_compile_definitions(MediaServer PRIVATE TAGLIB_STATIC)
#
# set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local")
# set(CMAKE_INSTALL_BINDIR "bin")
# set(CMAKE_INSTALL_LIBDIR "lib")
#
# install(TARGETS MediaServer
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )
#
# install(TARGETS
#     media_core
#     media_music
#     media_player
#     media_system
#     media_video
#     media_tagger
#     media_profiler
#     media_controllers
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )
#
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.json)
#     install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json
#         DESTINATION ${CMAKE_INSTALL_BINDIR}
#     )
# endif()
#
# set(SYSTEMD_USER_DIR "$ENV{HOME}/.config/systemd/user")
# set(SERVICE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/systemd/mediaserver.service)
#
# file(MAKE_DIRECTORY ${SYSTEMD_USER_DIR})
#
# if(EXISTS ${SERVICE_FILE})
#     configure_file(${SERVICE_FILE} ${SYSTEMD_USER_DIR}/mediaserver.service COPYONLY)
#     message(STATUS "Installed systemd service file to ${SYSTEMD_USER_DIR}")
#     message(STATUS "")
#     message(STATUS "To enable and start the service, run:")
#     message(STATUS "  systemctl --user daemon-reload")
#     message(STATUS "  systemctl --user enable mediaserver.service")
#     message(STATUS "  systemctl --user start mediaserver.service")
#     message(STATUS "")
#     message(STATUS "To check status:")
#     message(STATUS "  systemctl --user status mediaserver.service")
#     message(STATUS "")
#     message(STATUS "To view logs:")
#     message(STATUS "  journalctl --user -u mediaserver.service -f")
# else()
#     message(WARNING "mediaserver.service not found in ${CMAKE_CURRENT_SOURCE_DIR}")
# endif()
#
# set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
