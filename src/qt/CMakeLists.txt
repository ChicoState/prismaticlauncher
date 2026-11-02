qt_add_library(launcher-ui STATIC
    LauncherConfig.cpp LauncherConfig.h
    InstanceWindow.cpp InstanceWindow.h InstanceWindow.ui
    LauncherWindow.cpp LauncherWindow.h LauncherWindow.ui
    AddInstanceDialog.ui
    NexusPage.cpp NexusPage.h
    ModBrowser.cpp ModBrowser.h
    ../../resources.qrc
)
target_include_directories(launcher-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(launcher-ui PUBLIC Qt6::Widgets farmpack)
target_link_libraries(launcher-ui PUBLIC Qt6::Widgets nexus_fetch_list)
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

if(BUILD_TESTING)
    qt_add_executable(mod-pagination-tests ../../tests/ui/ModPaginationTests.cpp)
    target_link_libraries(mod-pagination-tests PRIVATE launcher-ui Qt6::Test)
    add_test(NAME mod-pagination-tests COMMAND mod-pagination-tests)
    set_tests_properties(mod-pagination-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(launcher-ui-tests ../../tests/ui/LauncherTests.cpp)
    target_link_libraries(launcher-ui-tests PRIVATE launcher-ui Qt6::Test nexus_fetch_list)
    add_test(NAME launcher-ui-tests COMMAND launcher-ui-tests)
    set_tests_properties(launcher-ui-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
