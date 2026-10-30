cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test Widgets)
find_package(nlohmann_json CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(src/farmpack)

if(BUILD_TESTING)
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
