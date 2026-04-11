cmake_minimum_required(VERSION 3.12)
project(MediaServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include(GNUInstallDirs)
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Drogon REQUIRED)
find_package(TagLib REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(LIBCURL REQUIRED libcurl)

get_target_property(TAGLIB_INCLUDE_DIRS TagLib::TagLib INTERFACE_INCLUDE_DIRECTORIES)

add_executable(MediaServer
    main.cpp
    controllers/VideoController.cpp
    controllers/MusicController.cpp
    controllers/PlayerController.cpp
    services/AlbumArtExtractor.cpp
    services/MusicMetadataExtractor.cpp
    services/PlayerService.cpp
    database/MusicDatabase.cpp
    profilers/Profiler.cpp
    tagger/TagEditor.cpp
    player/Player.cpp
)

target_link_libraries(MediaServer PRIVATE
    Drogon::Drogon
    TagLib::TagLib
    ${SQLITE3_LIBRARIES}
    ${MPV_LIBRARIES}
    ${LIBCURL_LIBRARIES}
)

target_include_directories(MediaServer PRIVATE
    ${SQLITE3_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
)

target_compile_definitions(MediaServer PRIVATE
    TAGLIB_STATIC
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.json)
    add_custom_command(
        TARGET MediaServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/config.json
            ${CMAKE_CURRENT_BINARY_DIR}/config.json
        COMMENT "Copying config.json to build directory"
    )
    message(STATUS "Will copy config.json to ${CMAKE_CURRENT_BINARY_DIR}/config.json")
else()
    message(WARNING "config.json not found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
