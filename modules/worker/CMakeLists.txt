add_library(geoframe_worker STATIC src/worker.cpp)
add_library(GeoFrame::worker ALIAS geoframe_worker)

target_include_directories(
    geoframe_worker
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    geoframe_worker
    PUBLIC
        GeoFrame::core
        GeoFrame::storage
        GeoFrame::media
        GeoFrame::db
)

geoframe_set_warnings(geoframe_worker)
