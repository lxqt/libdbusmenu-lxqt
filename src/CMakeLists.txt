set(dbusmenu_lxqt_SRCS
    dbusmenu_p.cpp
    dbusmenuexporter.cpp
    dbusmenuexporterdbus_p.cpp
    dbusmenuimporter.cpp
    dbusmenutypes_p.cpp
    dbusmenushortcut_p.cpp
    utils.cpp
    )

qt6_add_dbus_adaptor(dbusmenu_lxqt_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/com.canonical.dbusmenu.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/dbusmenuexporterdbus_p.h DBusMenuExporterDBus
    )

configure_file(dbusmenu_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/dbusmenu_version.h
    )

add_library(dbusmenu-lxqt SHARED ${dbusmenu_lxqt_SRCS})
set_target_properties(dbusmenu-lxqt PROPERTIES
    VERSION ${dbusmenu_lxqt_lib_VERSION}
    SOVERSION ${dbusmenu_lxqt_lib_SOVERSION}
    )

target_link_libraries(dbusmenu-lxqt
    Qt::Gui
    Qt::Core
        Qt::DBus
    Qt::Widgets
    )

# Make sure linking to the target adds dbusmenu-lxqt install directory
target_include_directories(dbusmenu-lxqt
    INTERFACE "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>")

install(TARGETS dbusmenu-lxqt
    EXPORT dbusmenu-lxqt-targets
    LIBRARY DESTINATION ${LIB_DESTINATION}
    RUNTIME DESTINATION bin
    )

install(EXPORT dbusmenu-lxqt-targets
    DESTINATION ${CMAKECONFIG_INSTALL_DIR})

install(DIRECTORY .
    DESTINATION ${INCLUDE_INSTALL_DIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*_p.h" EXCLUDE
    )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dbusmenu_version.h
    DESTINATION ${INCLUDE_INSTALL_DIR}
    )
