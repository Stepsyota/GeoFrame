add_executable(geoframe src/main.cpp)
target_link_libraries(geoframe PRIVATE GeoFrame::cli)

geoframe_set_warnings(geoframe)
