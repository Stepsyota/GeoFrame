find_package(Threads REQUIRED)

add_library(
    geoframe_worker
    STATIC
        src/hash_job_handler.cpp
        src/job_dispatcher.cpp
        src/metadata_job_handler.cpp
        src/scan_service.cpp
        src/worker_pool.cpp
)
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
        Threads::Threads
)

geoframe_set_warnings(geoframe_worker)
