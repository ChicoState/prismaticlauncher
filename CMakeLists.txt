cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(nexus_download
    prismatic-launcher/src/nexus/NexusHttpTransport.cpp
    prismatic-launcher/src/nexus/NexusModDownloader.cpp
)
target_include_directories(nexus_download
    PUBLIC
        prismatic-launcher/src
)
target_link_libraries(nexus_download
    PUBLIC
        Qt6::Core
        Qt6::Network
)

add_executable(prismatic-nexus-download prismatic-launcher/src/main.cpp)
target_link_libraries(prismatic-nexus-download PRIVATE nexus_download Qt6::Core)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(nexus-download-tests tests/nexus/NexusModDownloaderTest.cpp)
    target_link_libraries(nexus-download-tests PRIVATE nexus_download Qt6::Test)
    add_test(NAME nexus-download-tests COMMAND nexus-download-tests)
endif()
