# The root CMakeLists.txt includes this directory after finding Qt.
# Group installation detection, launching, and coordination in a reusable static library.
qt_add_library(launcher-smapi STATIC
    ManagedAssemblyVersionReader.cpp ManagedAssemblyVersionReader.h
    SmapiDetector.cpp SmapiDetector.h SmapiVersion.h
    SmapiLauncher.cpp SmapiLauncher.h
    SmapiManager.cpp SmapiManager.h
)
# Let consumers include the SMAPI headers directly when they link this target.
target_include_directories(launcher-smapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Propagate Qt Core because the public SMAPI interfaces expose Qt types.
target_link_libraries(launcher-smapi PUBLIC Qt6::Core PRIVATE nlohmann_json::nlohmann_json)
