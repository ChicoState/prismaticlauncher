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
find_package(nlohmann_json CONFIG REQUIRED)
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
add_subdirectory(src/farmpack)

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
    qt_add_executable(farmpack-tests tests/farmpack/FarmPackTests.cpp)
    target_link_libraries(farmpack-tests PRIVATE farmpack Qt6::Test)
    add_test(NAME farmpack-tests COMMAND farmpack-tests)

endif()

add_library(nexus_fetch_list STATIC
    src/Nexus_fetchList/Nexus_fetchList.cpp
    src/Nexus_fetchList/NexusModPages.cpp
    src/Nexus_fetchList/Nexus_fetchList.hpp
)
target_include_directories(nexus_fetch_list PUBLIC src/Nexus_fetchList)
target_link_libraries(nexus_fetch_list
    PUBLIC
        Qt6::Core
        Qt6::Network
        nlohmann_json::nlohmann_json
)

add_executable(nexus-fetch-list src/Nexus_fetchList/main.cpp)
target_link_libraries(nexus-fetch-list PRIVATE nexus_fetch_list)

if(BUILD_TESTING)
    add_executable(nexus-fetch-list-test tests/nexus_fetch_list_test.cpp)
    target_link_libraries(nexus-fetch-list-test PRIVATE nexus_fetch_list Qt6::Test)
    add_test(NAME nexus-fetch-list-test COMMAND nexus-fetch-list-test)
    qt_add_executable(nexus-mod-pages-test tests/nexus_mod_pages_test.cpp)
    target_link_libraries(nexus-mod-pages-test PRIVATE nexus_fetch_list Qt6::Test)
    add_test(NAME nexus-mod-pages-test COMMAND nexus-mod-pages-test)
endif()
add_subdirectory(src/qt)
