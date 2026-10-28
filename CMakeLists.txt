cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Test Widgets)
qt_standard_project_setup()

add_library(smapi-core STATIC
    src/smapi/ManagedAssemblyVersionReader.cpp
    src/smapi/ManagedAssemblyVersionReader.h
    src/smapi/SmapiDetector.cpp
    src/smapi/SmapiDetector.h
    src/smapi/SmapiLauncher.cpp
    src/smapi/SmapiLauncher.h
    src/smapi/SmapiManager.cpp
    src/smapi/SmapiManager.h
    src/smapi/SmapiVersion.h
)
target_include_directories(smapi-core PUBLIC src/smapi)
target_link_libraries(smapi-core PUBLIC Qt6::Core)

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

    qt_add_executable(smapi-tests tests/smapi/SmapiDetectorTests.cpp)
    target_link_libraries(smapi-tests PRIVATE smapi-core Qt6::Test)
    add_test(NAME smapi-tests COMMAND smapi-tests)
    set_tests_properties(smapi-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
