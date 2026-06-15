cmake_minimum_required(VERSION 3.16)
project(Archipelago VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Network DBus Test Concurrent)
find_library(UDEV_LIB NAMES udev REQUIRED)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

enable_testing()

set(ARCHIPELAGO_BACKEND_SOURCES
    backend/ArchipelagoConfig.h
    backend/ArchipelagoConfig.cpp
    backend/BatteryService.h
    backend/BatteryService.cpp
    backend/BrightnessService.h
    backend/BrightnessService.cpp
    backend/CompositorProvider.h
    backend/CompositorProvider.cpp
    backend/NiriCompositorService.h
    backend/NiriCompositorService.cpp
    backend/NotificationService.h
    backend/NotificationService.cpp
    backend/PluginScanner.h
    backend/PluginScanner.cpp
    backend/StyleTokens.h
    backend/StyleTokens.cpp
    backend/SysBackend.h
    backend/SysBackend.cpp
    backend/SystemServices.h
    backend/SystemServices.cpp
    backend/VolumeService.h
    backend/VolumeService.cpp
)

add_executable(archipelago_config_tests
    tests/archipelago_config_tests.cpp
    backend/ArchipelagoConfig.h
    backend/ArchipelagoConfig.cpp
)

target_include_directories(archipelago_config_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(archipelago_config_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
)

add_test(NAME archipelago_config COMMAND archipelago_config_tests)

add_executable(niri_compositor_service_tests
    tests/niri_compositor_service_tests.cpp
    backend/NiriCompositorService.h
    backend/NiriCompositorService.cpp
)

target_include_directories(niri_compositor_service_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(niri_compositor_service_tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Qml
    Qt6::Test
)

add_test(NAME niri_compositor_service COMMAND niri_compositor_service_tests)

add_executable(plugin_scanner_tests
    tests/plugin_scanner_tests.cpp
    backend/PluginScanner.h
    backend/PluginScanner.cpp
)

target_include_directories(plugin_scanner_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(plugin_scanner_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME plugin_scanner COMMAND plugin_scanner_tests)

add_executable(battery_service_tests
    tests/battery_service_tests.cpp
    backend/BatteryService.h
    backend/BatteryService.cpp
)

target_include_directories(battery_service_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(battery_service_tests PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
    ${UDEV_LIB}
)

add_test(NAME battery_service COMMAND battery_service_tests)

add_executable(volume_service_tests
    tests/volume_service_tests.cpp
    backend/VolumeService.h
    backend/VolumeService.cpp
)

target_include_directories(volume_service_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(volume_service_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME volume_service COMMAND volume_service_tests)

add_executable(brightness_service_tests
    tests/brightness_service_tests.cpp
    backend/BrightnessService.h
    backend/BrightnessService.cpp
)

target_include_directories(brightness_service_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(brightness_service_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME brightness_service COMMAND brightness_service_tests)

add_executable(notification_service_tests
    tests/notification_service_tests.cpp
    backend/NotificationService.h
    backend/NotificationService.cpp
)

target_include_directories(notification_service_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(notification_service_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME notification_service COMMAND notification_service_tests)

qt_add_qml_module(ArchipelagoBackend
    URI ArchipelagoBackend
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoBackend
    SOURCES
        ${ARCHIPELAGO_BACKEND_SOURCES}
)

target_include_directories(ArchipelagoBackend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(ArchipelagoBackend PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Network
    Qt6::DBus
    Qt6::Concurrent
    ${UDEV_LIB}
)

include(GNUInstallDirs)

install(FILES
    shell.qml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago
)

install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/archipelago
)

install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/archipelago
)

install(DIRECTORY qml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago
    FILES_MATCHING PATTERN "*.qml"
)

install(DIRECTORY examples
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago
    FILES_MATCHING PATTERN "*.json"
)

install(TARGETS ArchipelagoBackend
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS ArchipelagoBackendplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/qml/ArchipelagoBackend
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoBackend/qmldir
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoBackend/ArchipelagoBackend.qmltypes
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/qml/ArchipelagoBackend
)

install(PROGRAMS archipelago-launcher DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME archipelago)
install(FILES archipelago.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES archipelago.service DESTINATION /usr/lib/systemd/user)
