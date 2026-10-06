cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(prismatic_launcher
    prismatic-launcher/src/main.cpp
    prismatic-launcher/src/smapi/SmapiInstaller.cpp
    prismatic-launcher/src/smapi/SmapiLauncher.cpp
)

target_link_libraries(prismatic_launcher PRIVATE Qt6::Core)

include(CTest)

