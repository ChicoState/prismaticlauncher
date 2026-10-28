# Keep windows, dialogs, configuration, and shared resources in a library usable by the app and tests.
qt_add_library(launcher-ui STATIC
    LauncherConfig.cpp LauncherConfig.h
    LogWindow.cpp LogWindow.h
    InstanceWindow.cpp InstanceWindow.h InstanceWindow.ui
    LauncherWindow.cpp LauncherWindow.h LauncherWindow.ui
    AddInstanceDialog.ui
    ../../resources.qrc

)
# Let targets linking launcher-ui include its headers without repeating this directory.
target_include_directories(launcher-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Propagate Qt Widgets because the library's public interfaces use Qt widget types.
target_link_libraries(launcher-ui PUBLIC Qt6::Widgets)
# Embed the stylesheet and default settings under the :/ui resource path.
qt_add_resources(launcher-ui ui-resources
    PREFIX "/ui"
    FILES style.qss config/defaults.ini
)

# Keep the application entry point separate from the UI library used by tests.
qt_add_executable(prismatic-launch main.cpp)
target_link_libraries(prismatic-launch PRIVATE launcher-ui)
# Use a GUI executable on Windows and an application bundle on macOS.
# Place the executable in the build tree's bin directory.
set_target_properties(prismatic-launch PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
