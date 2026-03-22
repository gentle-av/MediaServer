cmake_minimum_required(VERSION 3.12)
project(MediaExplorerDrogon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Drogon REQUIRED)
find_package(TagLib REQUIRED)

add_executable(media_explorer_drogon
    main.cpp
    controllers/VideoController.cpp
    controllers/MusicController.cpp
    services/AlbumArtExtractor.cpp
    services/MusicMetadataExtractor.cpp
)

target_link_libraries(media_explorer_drogon PRIVATE
    Drogon::Drogon
    TagLib::TagLib
)

install(TARGETS media_explorer_drogon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/views/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/media-explorer-drogon/views
    COMPONENT Web
    PATTERN "*.html"
    PATTERN "*.css"
    PATTERN "*.js"
    PATTERN "*.png"
    PATTERN "*.jpg"
    PATTERN "*.svg"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/media-explorer-drogon
    COMPONENT Documentation
    OPTIONAL
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/media-explorer-drogon.in
    ${CMAKE_CURRENT_BINARY_DIR}/media-explorer-drogon
    @ONLY
)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/media-explorer-drogon
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/media-explorer-drogon.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/media-explorer-drogon.service
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/media-explorer-drogon.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
    COMPONENT Systemd
    OPTIONAL
)

install(CODE "
    message(STATUS \"Creating log directory...\")
    file(MAKE_DIRECTORY /var/log/media-explorer-drogon)
" COMPONENT Runtime)

set(CPACK_PACKAGE_NAME "media-explorer-drogon")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Media Explorer Web Interface")
set(CPACK_PACKAGE_VENDOR "Your Name")
set(CPACK_PACKAGE_CONTACT "your.email@example.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/yourusername/media-explorer-drogon")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/media-explorer-drogon
    COMPONENT Documentation
    OPTIONAL
)

set(CPACK_GENERATOR "DEB;RPM;TGZ")

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libdrogon1 (>= 1.8.0), libjsoncpp25, libssl3")
set(CPACK_DEBIAN_PACKAGE_SECTION "web")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME "media-explorer-drogon-${CPACK_PACKAGE_VERSION}.deb")

set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_REQUIRES "drogon >= 1.8.0, jsoncpp, openssl")
set(CPACK_RPM_FILE_NAME "media-explorer-drogon-${CPACK_PACKAGE_VERSION}.rpm")

include(CPack)

enable_testing()

add_test(NAME start_server
    COMMAND media_explorer_drogon
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(NAME api_test
    COMMAND curl -f http://localhost:8083/api/list -X POST -H "Content-Type: application/json" -d '{}'
)
