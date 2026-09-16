add_library(
    geoframe_core
    STATIC
        src/version.cpp
        src/event_bus.cpp
        src/map_clusterer.cpp
        src/progress_tracker.cpp
)
add_library(GeoFrame::core ALIAS geoframe_core)

target_include_directories(
    geoframe_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

geoframe_set_warnings(geoframe_core)
