set(qtapp_SRCS
    main.cpp
    )

add_executable(dbusmenubench-lxqtapp ${qtapp_SRCS})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_BINARY_DIR}/../../src
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6DBus_INCLUDE_DIRS}
    ${QJSON_INCLUDE_DIR}
    )

target_link_libraries(dbusmenubench-lxqtapp
    dbusmenu-lxqt
    ${Qt6Gui_LIBRARIES}
    ${Qt6Core_LIBRARIES}
    ${Qt6DBus_LIBRARIES}
    ${Qt6Widgets_LIBRARIES}
    ${QJSON_LIBRARIES}
    )
