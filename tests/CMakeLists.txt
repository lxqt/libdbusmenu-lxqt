# Macros to create "check" target
set(_test_executable_list "")

macro(add_test_executable _executable)
    add_test(${_executable} ${_executable})
    set(_test_executable_list "${_test_executable_list};${_executable}")
    add_executable(${_executable} ${ARGN})
endmacro(add_test_executable)

# Call this at the end
macro(create_check_target)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose
        DEPENDS ${_test_executable_list})
endmacro(create_check_target)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QT_QTTEST_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    )

set(test_LIBRARIES
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTTEST_LIBRARIES}
    dbusmenu-qt
    )

enable_testing()

# dbusmenutest
set(dbusmenutest_SRCS
    dbusmenutest.cpp
    )

qt4_automoc(${dbusmenutest_SRCS})

add_test_executable(dbusmenutest ${dbusmenutest_SRCS})

target_link_libraries(dbusmenutest
    ${test_LIBRARIES}
    )

# Keep this at the end
create_check_target()