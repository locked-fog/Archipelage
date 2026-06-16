cmake_minimum_required(VERSION 3.16)
project(ArchipelagoMediaControlCenter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick DBus Test)

include(GNUInstallDirs)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

enable_testing()

set(MEDIA_MODULE_SOURCES
    backend/MediaService.cpp
    backend/MediaService.h
    backend/MprisModel.cpp
    backend/MprisModel.h
)

qt_add_qml_module(ArchipelagoPluginsMedia
    URI ArchipelagoPlugins.Media
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Media
    SOURCES
        ${MEDIA_MODULE_SOURCES}
)

if(TARGET ArchipelagoPluginsMediaplugin)
    set_target_properties(ArchipelagoPluginsMediaplugin PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

target_link_libraries(ArchipelagoPluginsMedia PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
)

target_include_directories(ArchipelagoPluginsMedia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

add_executable(media_model_tests
    tests/media_model_tests.cpp
    backend/MprisModel.cpp
    backend/MprisModel.h
)

target_include_directories(media_model_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(media_model_tests PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
)

add_test(NAME media_model COMMAND media_model_tests)

add_executable(media_qml_tests
    tests/media_qml_tests.cpp
)

target_compile_definitions(media_qml_tests PRIVATE
    MEDIA_PLUGIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    MEDIA_PLUGIN_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(media_qml_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)

add_test(NAME media_qml COMMAND media_qml_tests)

install(DIRECTORY ui/media
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/plugins
    FILES_MATCHING
        PATTERN "*.qml"
        PATTERN "manifest.json"
)

install(TARGETS ArchipelagoPluginsMedia
    LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Media
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET ArchipelagoPluginsMediaplugin)
    install(TARGETS ArchipelagoPluginsMediaplugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Media
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Media/qmldir
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Media
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Media/ArchipelagoPluginsMedia.qmltypes
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Media
    OPTIONAL
)
