cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)
find_package(nlohmann_json CONFIG REQUIRED)

qt_standard_project_setup()

add_library(nexus_fetch_list STATIC
    src/Nexus_fetchList/Nexus_fetchList.cpp
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
endif()
