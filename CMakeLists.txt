cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(prismatic-launcher/ui)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(launcher-ui-tests tests/ui/LauncherTests.cpp)
    target_link_libraries(launcher-ui-tests PRIVATE launcher-ui Qt6::Test)
    add_test(NAME launcher-ui COMMAND launcher-ui-tests)
    set_tests_properties(launcher-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
