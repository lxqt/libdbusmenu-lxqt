cmake_minimum_required(VERSION 3.18.0)
project(dbusmenu-lxqt)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

# Build options
option(WITH_DOC "Build documentation (requires Doxygen)" OFF)

# Versions
## Package version
set(dbusmenu_lxqt_VERSION_MAJOR 0)
set(dbusmenu_lxqt_VERSION_MINOR 3)
set(dbusmenu_lxqt_VERSION_PATCH 0)
set(dbusmenu_lxqt_VERSION ${dbusmenu_lxqt_VERSION_MAJOR}.${dbusmenu_lxqt_VERSION_MINOR}.${dbusmenu_lxqt_VERSION_PATCH})

## Lib version
### Bump this one when a binary-incompatible change is introduced
set(dbusmenu_lxqt_lib_SOVERSION 0)

### Bump this one when the API is extended in a binary-compatible way
set(dbusmenu_lxqt_lib_API_VERSION 3)

### Bump this one when changes do not extend the API
set(dbusmenu_lxqt_lib_PATCH_VERSION 0)

set(dbusmenu_lxqt_lib_VERSION ${dbusmenu_lxqt_lib_SOVERSION}.${dbusmenu_lxqt_lib_API_VERSION}.${dbusmenu_lxqt_lib_PATCH_VERSION})

if(WITH_DOC)
    find_package(Doxygen REQUIRED)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Default to hidden visibility for symbols
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

include (CheckCXXCompilerFlag)
# Check some compiler flags
check_cxx_compiler_flag(-Woverloaded-virtual __DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
if (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
endif (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)

check_cxx_compiler_flag(-Wall __DBUSMENU_HAVE_W_ALL)
if (__DBUSMENU_HAVE_W_ALL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif (__DBUSMENU_HAVE_W_ALL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
set(LIB_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
set(CMAKECONFIG_INSTALL_DIR "${LIB_DESTINATION}/cmake/dbusmenu-lxqt")
set(INCLUDE_INSTALL_DIR "include/dbusmenu-lxqt")

configure_file(dbusmenu-lxqt.pc.in ${CMAKE_BINARY_DIR}/dbusmenu-lxqt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dbusmenu-lxqt.pc
        DESTINATION ${LIB_DESTINATION}/pkgconfig
    )

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)

if(WITH_DOC)
    configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(doc ALL doxygen
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

    install(DIRECTORY ${CMAKE_BINARY_DIR}/html/
        DESTINATION share/doc/libdbusmenu-lxqt-doc
        )
endif(WITH_DOC)

# Generate dbusmenu-lxqt-config* files
configure_package_config_file(
    dbusmenu-lxqt-config.cmake.in
    ${CMAKE_BINARY_DIR}/dbusmenu-lxqt-config.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
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
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Devel
    )
