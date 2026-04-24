cmake_minimum_required(VERSION 3.12)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

find_package(Drogon REQUIRED)
find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(FFMPEGTHUMBNAILER REQUIRED libffmpegthumbnailer)

# Собираем всё в один исполняемый файл
add_executable(MediaServer
    main.cpp
    controllers/VideoController.cpp
    controllers/MusicController.cpp
    controllers/PlayerController.cpp
    services/AlbumArtExtractor.cpp
    services/MusicMetadataExtractor.cpp
    services/PlayerService.cpp
    services/ThumbnailCache.cpp
    services/ThumbnailExtractor.cpp
    database/MusicDatabase.cpp
    profilers/Profiler.cpp
    tagger/TagEditor.cpp
    player/Player.cpp
)

target_link_libraries(MediaServer
    Drogon::Drogon
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${FFMPEGTHUMBNAILER_LIBRARIES}
    pthread
    dl
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
