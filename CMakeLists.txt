cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
qt_standard_project_setup()

add_subdirectory(src/qt)

if(BUILD_TESTING)
    qt_add_executable(launcher-ui-tests tests/ui/LauncherTests.cpp
        resources/styles.txt

    )
    target_link_libraries(launcher-ui-tests PRIVATE launcher-ui Qt6::Test)
    add_test(NAME launcher-ui-tests COMMAND launcher-ui-tests)
    set_tests_properties(launcher-ui-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
