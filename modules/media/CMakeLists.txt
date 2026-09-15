include(FetchContent)

find_package(OpenSSL REQUIRED)
find_package(exiv2 CONFIG QUIET)

if(NOT TARGET Exiv2::exiv2lib)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(EXIV2_BUILD_EXIV2_COMMAND OFF CACHE BOOL "" FORCE)
    set(EXIV2_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(EXIV2_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(EXIV2_ENABLE_BROTLI OFF CACHE BOOL "" FORCE)
    set(EXIV2_ENABLE_INIH OFF CACHE BOOL "" FORCE)
    set(EXIV2_ENABLE_VIDEO OFF CACHE BOOL "" FORCE)
    set(EXIV2_ENABLE_XMP OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        exiv2
        URL https://github.com/Exiv2/exiv2/archive/refs/tags/v0.28.9.tar.gz
        URL_HASH SHA256=700b76b97695b2fab4ef8c79619c68ae57d09e0c130724791cafbd39e0eb4aef
    )
    FetchContent_MakeAvailable(exiv2)
endif()

if(NOT TARGET reproc++)
    set(REPROC++ ON CACHE BOOL "" FORCE)
    set(REPROC_TEST OFF CACHE BOOL "" FORCE)
    set(REPROC_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        reproc
        URL https://github.com/DaanDeMeyer/reproc/archive/refs/tags/v14.2.7.tar.gz
    )
    FetchContent_MakeAvailable(reproc)
endif()

add_library(
    geoframe_media
    STATIC
        src/metadata_extractor.cpp
        src/preview_generator.cpp
        src/process_runner.cpp
        src/sha256.cpp
        src/video_extractor.cpp
)
add_library(GeoFrame::media ALIAS geoframe_media)

target_include_directories(
    geoframe_media
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_BINARY_DIR}"
)
target_link_libraries(
    geoframe_media
    PUBLIC
        GeoFrame::core
        GeoFrame::storage
        OpenSSL::Crypto
        Exiv2::exiv2lib
        reproc++
)

geoframe_set_warnings(geoframe_media)
