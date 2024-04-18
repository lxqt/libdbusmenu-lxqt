cmake_minimum_required(VERSION 3.18.0)
project(dbusmenu-lxqt)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

# Build options
option(WITH_DOC "Build documentation (requires Doxygen)" OFF)

# Versions
## Package version
set(dbusmenu_lxqt_VERSION_MAJOR 0)
set(dbusmenu_lxqt_VERSION_MINOR 1)
set(dbusmenu_lxqt_VERSION_PATCH 0)
set(dbusmenu_lxqt_VERSION ${dbusmenu_lxqt_VERSION_MAJOR}.${dbusmenu_lxqt_VERSION_MINOR}.${dbusmenu_lxqt_VERSION_PATCH})

## Lib version
### Bump this one when a binary-incompatible change is introduced
set(dbusmenu_lxqt_lib_SOVERSION 0)

### Bump this one when the API is extended in a binary-compatible way
set(dbusmenu_lxqt_lib_API_VERSION 1)

### Bump this one when changes do not extend the API
set(dbusmenu_lxqt_lib_PATCH_VERSION 0)

set(dbusmenu_lxqt_lib_VERSION ${dbusmenu_lxqt_lib_SOVERSION}.${dbusmenu_lxqt_lib_API_VERSION}.${dbusmenu_lxqt_lib_PATCH_VERSION})

if(WITH_DOC)
    find_package(Doxygen REQUIRED)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus)
include_directories(${Qt6Widgets_INCLUDE_DIRS} ${Qt6DBus_INCLUDE_DIRS})
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include (CheckCXXCompilerFlag)
# Check some compiler flags
check_cxx_compiler_flag(-fvisibility=hidden __DBUSMENU_HAVE_GCC_VISIBILITY)
if (__DBUSMENU_HAVE_GCC_VISIBILITY AND NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif (__DBUSMENU_HAVE_GCC_VISIBILITY AND NOT WIN32)

check_cxx_compiler_flag(-Woverloaded-virtual __DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
if (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
endif (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
set(LIB_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
set(CMAKECONFIG_INSTALL_DIR "${LIB_DESTINATION}/cmake/dbusmenu-lxqt")
set(INCLUDE_INSTALL_DIR "include/dbusmenu-lxqt")

# dist targets
set(ARCHIVE_NAME libdbusmenu-lxqt-${dbusmenu_lxqt_VERSION})
add_custom_target(dist
    COMMAND bzr export --root=${ARCHIVE_NAME} ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

add_custom_target(distcheck
    COMMAND cd ${CMAKE_BINARY_DIR}
    && rm -rf ${ARCHIVE_NAME}
    && tar xf ${ARCHIVE_NAME}.tar.bz2
    && mkdir ${ARCHIVE_NAME}/build
    && cd ${ARCHIVE_NAME}/build
    && cmake -DCMAKE_INSTALL_PREFIX=../install ..
    && make
    && make install
    && make check
    )
add_dependencies(distcheck dist)

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
