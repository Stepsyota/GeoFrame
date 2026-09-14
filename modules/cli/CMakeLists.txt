add_library(geoframe_cli STATIC src/cli.cpp)
add_library(GeoFrame::cli ALIAS geoframe_cli)

target_include_directories(
    geoframe_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    geoframe_cli
    PUBLIC
        GeoFrame::core
        GeoFrame::storage
        GeoFrame::media
        GeoFrame::db
        GeoFrame::worker
        GeoFrame::server
)

geoframe_set_warnings(geoframe_cli)
