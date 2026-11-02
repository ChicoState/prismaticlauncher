qt_add_library(farmpack STATIC
    FarmPack.cpp FarmPack.h
)
target_include_directories(farmpack PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(farmpack PUBLIC Qt6::Core)
