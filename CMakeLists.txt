# Set the minimum CMake version and identify the application version and language.
cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher VERSION 0.1.0 LANGUAGES CXX)

# Generate C++ code from Qt Designer forms (.ui), QObject declarations, and resources (.qrc).
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Require C++20 on every compiler and disable compiler-specific language extensions.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Provide the BUILD_TESTING option (on by default) and enable test registration with CTest.
include(CTest)

# Load the Qt Widgets dependencies used by the desktop UI and apply Qt's project defaults.
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Define the application and its UI library, then the reusable SMAPI services library.
add_subdirectory(src/qt)
add_subdirectory(src/smapi)

if(BUILD_TESTING)
    # Require Qt Test only when building the test executables.
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # Link the UI tests to the UI library and register their executable with CTest.
    qt_add_executable(launcher-ui-tests tests/ui/LauncherTests.cpp)
    target_link_libraries(launcher-ui-tests PRIVATE launcher-ui Qt6::Test)
    add_test(NAME launcher-ui-tests COMMAND launcher-ui-tests)

    # Let widget tests run on CI machines without a desktop display.
    set_tests_properties(launcher-ui-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Exercise console parsing and filtering without launching a real game.
    qt_add_executable(log-window-tests tests/ui/LogWindowTests.cpp)
    target_link_libraries(log-window-tests PRIVATE launcher-ui Qt6::Test)
    add_test(NAME log-window-tests COMMAND log-window-tests)
    set_tests_properties(log-window-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Test SMAPI services independently of the UI; this suite needs no widget display backend.
    qt_add_executable(smapi-tests tests/smapi/SmapiTests.cpp)
    target_link_libraries(smapi-tests PRIVATE launcher-smapi Qt6::Test)
    add_test(NAME smapi-tests COMMAND smapi-tests)
endif()
