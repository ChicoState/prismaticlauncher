cmake_minimum_required(VERSION 3.28)

project(PrismaticLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# Application targets intentionally do not exist yet. Product implementation
# should add targets under src/ and register its tests with add_test().
