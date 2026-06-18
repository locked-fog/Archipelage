cmake_minimum_required(VERSION 3.16)
project(ArchipelagoSystemControlCenter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick DBus Test)

include(GNUInstallDirs)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

enable_testing()

set(SYSTEM_CONTROL_MODULE_SOURCES
    backend/AudioRouteService.cpp
    backend/AudioRouteService.h
    backend/BluetoothService.cpp
    backend/BluetoothService.h
    backend/ConnectivityService.cpp
    backend/ConnectivityService.h
    backend/DisplayService.cpp
    backend/DisplayService.h
    backend/PowerProfileService.cpp
    backend/PowerProfileService.h
)

qt_add_qml_module(ArchipelagoPluginsSystemControl
    URI ArchipelagoPlugins.SystemControl
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/SystemControl
    SOURCES
        ${SYSTEM_CONTROL_MODULE_SOURCES}
)

if(TARGET ArchipelagoPluginsSystemControlplugin)
    set_target_properties(ArchipelagoPluginsSystemControlplugin PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

target_link_libraries(ArchipelagoPluginsSystemControl PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
)

target_include_directories(ArchipelagoPluginsSystemControl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

add_executable(system_control_backend_tests
    tests/system_control_backend_tests.cpp
    backend/AudioRouteService.cpp
    backend/AudioRouteService.h
    backend/BluetoothService.cpp
    backend/BluetoothService.h
    backend/ConnectivityService.cpp
    backend/ConnectivityService.h
    backend/DisplayService.cpp
    backend/DisplayService.h
    backend/PowerProfileService.cpp
    backend/PowerProfileService.h
)

target_include_directories(system_control_backend_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(system_control_backend_tests PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
)

add_test(NAME system_control_backend COMMAND system_control_backend_tests)

add_executable(system_control_qml_tests
    tests/system_control_qml_tests.cpp
)

target_compile_definitions(system_control_qml_tests PRIVATE
    SYSTEM_CONTROL_PLUGIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    SYSTEM_CONTROL_PLUGIN_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(system_control_qml_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)

add_test(NAME system_control_qml COMMAND system_control_qml_tests)

install(DIRECTORY ui/system-control
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/plugins
    FILES_MATCHING
        PATTERN "*.qml"
        PATTERN "manifest.json"
)

install(TARGETS ArchipelagoPluginsSystemControl
    LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/SystemControl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET ArchipelagoPluginsSystemControlplugin)
    install(TARGETS ArchipelagoPluginsSystemControlplugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/SystemControl
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/SystemControl/qmldir
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/SystemControl
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/SystemControl/ArchipelagoPluginsSystemControl.qmltypes
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/SystemControl
    OPTIONAL
)
