find_package(OpenSSL REQUIRED)

add_library(geoframe_media STATIC src/sha256.cpp)
add_library(GeoFrame::media ALIAS geoframe_media)

target_include_directories(
    geoframe_media
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    geoframe_media
    PUBLIC
        GeoFrame::core
        GeoFrame::storage
        OpenSSL::Crypto
)

geoframe_set_warnings(geoframe_media)
