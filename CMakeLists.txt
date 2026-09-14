cmake_minimum_required(VERSION 3.24)

project(
    GeoFrame
    VERSION 0.1.0
    DESCRIPTION "Self-hosted photo and video library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GEOFRAME_BUILD_TESTS "Build GeoFrame tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GeoFrameWarnings)

add_subdirectory(modules/core)
add_subdirectory(modules/storage)
add_subdirectory(modules/media)
add_subdirectory(modules/db)
add_subdirectory(modules/worker)
add_subdirectory(modules/server)
add_subdirectory(modules/cli)
add_subdirectory(apps/geoframe)

if(GEOFRAME_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
