qt_add_library(launcher-ui STATIC
    LauncherConfig.cpp LauncherConfig.h
    InstanceWindow.cpp InstanceWindow.h InstanceWindow.ui
    LauncherWindow.cpp LauncherWindow.h LauncherWindow.ui
    AddInstanceDialog.ui
    ../../resources.qrc

)
target_include_directories(launcher-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(launcher-ui PUBLIC Qt6::Widgets farmpack)
qt_add_resources(launcher-ui ui-resources
    PREFIX "/ui"
    FILES style.qss config/defaults.ini
)

qt_add_executable(prismatic-launch main.cpp)
target_link_libraries(prismatic-launch PRIVATE launcher-ui)
set_target_properties(prismatic-launch PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
