cmake_minimum_required(VERSION 3.16)
project(ArchipelagoNotificationCenter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick DBus Test)

include(GNUInstallDirs)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

enable_testing()

set(NOTIFICATION_CENTER_MODULE_SOURCES
    backend/NotificationCenterService.cpp
    backend/NotificationCenterService.h
)

qt_add_qml_module(ArchipelagoPluginsNotifications
    URI ArchipelagoPlugins.Notifications
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Notifications
    SOURCES
        ${NOTIFICATION_CENTER_MODULE_SOURCES}
)

if(TARGET ArchipelagoPluginsNotificationsplugin)
    set_target_properties(ArchipelagoPluginsNotificationsplugin PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

target_link_libraries(ArchipelagoPluginsNotifications PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
)

target_include_directories(ArchipelagoPluginsNotifications PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

add_executable(notification_center_backend_tests
    tests/notification_center_backend_tests.cpp
    backend/NotificationCenterService.cpp
    backend/NotificationCenterService.h
)

target_include_directories(notification_center_backend_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(notification_center_backend_tests PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Qml
    Qt6::Test
)

add_test(NAME notification_center_backend COMMAND notification_center_backend_tests)

add_executable(notification_center_qml_tests
    tests/notification_center_qml_tests.cpp
)

target_compile_definitions(notification_center_qml_tests PRIVATE
    NOTIFICATION_CENTER_PLUGIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    NOTIFICATION_CENTER_PLUGIN_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(notification_center_qml_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)

add_test(NAME notification_center_qml COMMAND notification_center_qml_tests)

install(DIRECTORY ui/notifications
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/plugins
    FILES_MATCHING
        PATTERN "*.qml"
        PATTERN "manifest.json"
)

install(TARGETS ArchipelagoPluginsNotifications
    LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Notifications
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET ArchipelagoPluginsNotificationsplugin)
    install(TARGETS ArchipelagoPluginsNotificationsplugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Notifications
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Notifications/qmldir
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Notifications
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArchipelagoPlugins/Notifications/ArchipelagoPluginsNotifications.qmltypes
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archipelago/qml/ArchipelagoPlugins/Notifications
    OPTIONAL
)
