cmake_minimum_required(VERSION 3.12)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local")
set(CMAKE_INSTALL_BINDIR "bin")
set(CMAKE_INSTALL_LIBDIR "lib/MediaServer")
set(CMAKE_INSTALL_INCLUDEDIR "include")

include(GNUInstallDirs)

set(CMAKE_INSTALL_RPATH "$ENV{HOME}/.local/lib/MediaServer")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Drogon REQUIRED)
find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(FFMPEGTHUMBNAILER REQUIRED libffmpegthumbnailer)

add_library(VideoController SHARED controllers/VideoController.cpp)
add_library(MusicController SHARED controllers/MusicController.cpp)
add_library(PlayerController SHARED controllers/PlayerController.cpp)

add_library(AlbumArtExtractor SHARED services/AlbumArtExtractor.cpp)
add_library(MusicMetadataExtractor SHARED services/MusicMetadataExtractor.cpp)
add_library(PlayerService SHARED services/PlayerService.cpp)
add_library(ThumbnailCache SHARED services/ThumbnailCache.cpp)
add_library(ThumbnailExtractor SHARED services/ThumbnailExtractor.cpp)

add_library(MusicDatabase SHARED database/MusicDatabase.cpp)
add_library(Profiler SHARED profilers/Profiler.cpp)
add_library(TagEditor SHARED tagger/TagEditor.cpp)
add_library(Player SHARED player/Player.cpp)

function(setup_library_target TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE
        Drogon::Drogon
        TagLib::TagLib
        ${SQLITE3_LIBRARIES}
        ${MPV_LIBRARIES}
        ${LIBCURL_LIBRARIES}
        ${FFMPEGTHUMBNAILER_LIBRARIES}
    )

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${SQLITE3_INCLUDE_DIRS}
            ${TAGLIB_INCLUDE_DIRS}
            ${MPV_INCLUDE_DIRS}
            ${LIBCURL_INCLUDE_DIRS}
            ${FFMPEGTHUMBNAILER_INCLUDE_DIRS}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/MediaServer>
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE TAGLIB_STATIC)

    install(TARGETS ${TARGET_NAME}
        EXPORT MediaServerTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

setup_library_target(VideoController)
setup_library_target(MusicController)
setup_library_target(PlayerController)
setup_library_target(AlbumArtExtractor)
setup_library_target(MusicMetadataExtractor)
setup_library_target(PlayerService)
setup_library_target(ThumbnailCache)
setup_library_target(ThumbnailExtractor)
setup_library_target(MusicDatabase)
setup_library_target(Profiler)
setup_library_target(TagEditor)
setup_library_target(Player)

install(DIRECTORY controllers/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/controllers
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY services/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/services
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY database/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/database
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY profilers/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/profilers
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY tagger/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/tagger
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY player/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaServer/player
    FILES_MATCHING PATTERN "*.h"
)

add_executable(MediaServer main.cpp)

target_link_libraries(MediaServer PRIVATE
    VideoController
    MusicController
    PlayerController
    AlbumArtExtractor
    MusicMetadataExtractor
    PlayerService
    ThumbnailCache
    ThumbnailExtractor
    MusicDatabase
    Profiler
    TagEditor
    Player
    Drogon::Drogon
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${FFMPEGTHUMBNAILER_LIBRARIES}
)

target_include_directories(MediaServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${FFMPEGTHUMBNAILER_INCLUDE_DIRS}
)

target_compile_definitions(MediaServer PRIVATE TAGLIB_STATIC)

install(TARGETS MediaServer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.json)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(EXPORT MediaServerTargets
    FILE MediaServerTargets.cmake
    NAMESPACE MediaServer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MediaServer
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MediaServerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/MediaServerConfig.cmake
    "@PACKAGE_INIT@
include(\${CMAKE_CURRENT_LIST_DIR}/MediaServerTargets.cmake)

get_target_property(MediaServer_INCLUDE_DIRS MediaServer::VideoController INTERFACE_INCLUDE_DIRECTORIES)
set(MediaServer_LIBRARIES
    MediaServer::VideoController
    MediaServer::MusicController
    MediaServer::PlayerController
    MediaServer::AlbumArtExtractor
    MediaServer::MusicMetadataExtractor
    MediaServer::PlayerService
    MediaServer::ThumbnailCache
    MediaServer::ThumbnailExtractor
    MediaServer::MusicDatabase
    MediaServer::Profiler
    MediaServer::TagEditor
    MediaServer::Player
)
set(MediaServer_LIBRARY_DIRS \"${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_LIBDIR}\")
"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MediaServerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MediaServerConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MediaServer
)
