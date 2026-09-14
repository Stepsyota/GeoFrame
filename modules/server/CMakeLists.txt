add_library(geoframe_server STATIC src/server.cpp)
add_library(GeoFrame::server ALIAS geoframe_server)

target_include_directories(
    geoframe_server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(geoframe_server PUBLIC GeoFrame::core GeoFrame::db)

geoframe_set_warnings(geoframe_server)
