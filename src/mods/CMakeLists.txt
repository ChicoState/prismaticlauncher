add_library(mod-installer STATIC ModInstaller.cpp ModInstaller.h)
target_include_directories(mod-installer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mod-installer PUBLIC Qt6::Core PRIVATE nlohmann_json::nlohmann_json LibArchive::LibArchive)

if(BUILD_TESTING)
    qt_add_executable(mod-installer-tests ../../tests/mods/ModInstallerTests.cpp)
    target_link_libraries(mod-installer-tests PRIVATE mod-installer Qt6::Test LibArchive::LibArchive)
    add_test(NAME mod-installer-tests COMMAND mod-installer-tests)
endif()
