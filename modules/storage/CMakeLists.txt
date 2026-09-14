add_library(geoframe_storage STATIC src/storage.cpp)
add_library(GeoFrame::storage ALIAS geoframe_storage)

target_include_directories(
    geoframe_storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(geoframe_storage PUBLIC GeoFrame::core)

geoframe_set_warnings(geoframe_storage)
