cmake_minimum_required(VERSION 3.18.0)
project(dbusmenu-lxqt)

# Versions
## Package version
set(dbusmenu_lxqt_VERSION_MAJOR 0)
set(dbusmenu_lxqt_VERSION_MINOR 4)
set(dbusmenu_lxqt_VERSION_PATCH 0)
set(dbusmenu_lxqt_VERSION ${dbusmenu_lxqt_VERSION_MAJOR}.${dbusmenu_lxqt_VERSION_MINOR}.${dbusmenu_lxqt_VERSION_PATCH})

set(LXQTBT_MINIMUM_VERSION "2.4.0")
set(QT_MINIMUM_VERSION "6.6.0")

## Lib version
### Bump this one when a binary-incompatible change is introduced
set(dbusmenu_lxqt_lib_SOVERSION 0)

### Bump this one when the API is extended in a binary-compatible way
set(dbusmenu_lxqt_lib_API_VERSION 4)

### Bump this one when changes do not extend the API
set(dbusmenu_lxqt_lib_PATCH_VERSION 0)

set(dbusmenu_lxqt_lib_VERSION ${dbusmenu_lxqt_lib_SOVERSION}.${dbusmenu_lxqt_lib_API_VERSION}.${dbusmenu_lxqt_lib_PATCH_VERSION})

find_package(lxqt2-build-tools ${LXQTBT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6 ${QT_MINIMUM_VERSION} REQUIRED COMPONENTS Core Widgets DBus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include (LXQtCompilerSettings NO_POLICY_SCOPE)
include (CheckCXXCompilerFlag)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/dbusmenu-lxqt")

configure_file(dbusmenu-lxqt.pc.in ${CMAKE_BINARY_DIR}/dbusmenu-lxqt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dbusmenu-lxqt.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

add_subdirectory(src)
add_subdirectory(tests)

# Generate dbusmenu-lxqt-config* files
configure_package_config_file(
    dbusmenu-lxqt-config.cmake.in
    ${CMAKE_BINARY_DIR}/dbusmenu-lxqt-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dbusmenu-lxqt
    PATH_VARS INCLUDE_INSTALL_DIR
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/dbusmenu-lxqt-config-version.cmake
    VERSION ${dbusmenu_lxqt_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install dbusmenu-lxqt-config* files
install(FILES
    ${CMAKE_BINARY_DIR}/dbusmenu-lxqt-config.cmake
    ${CMAKE_BINARY_DIR}/dbusmenu-lxqt-config-version.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dbusmenu-lxqt"
    COMPONENT Devel
)
