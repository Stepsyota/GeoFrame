include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(
    geoframe_core_tests
    core/version_test.cpp
    core/map_clusterer_test.cpp
)
target_link_libraries(geoframe_core_tests PRIVATE GeoFrame::core GTest::gtest_main)
geoframe_set_warnings(geoframe_core_tests)

gtest_discover_tests(geoframe_core_tests)

add_executable(
    geoframe_db_tests
    db/sqlite_asset_repository_test.cpp
    db/sqlite_job_repository_test.cpp
)
target_link_libraries(geoframe_db_tests PRIVATE GeoFrame::db GTest::gtest_main)
geoframe_set_warnings(geoframe_db_tests)

gtest_discover_tests(geoframe_db_tests)

add_executable(geoframe_storage_tests storage/directory_scanner_test.cpp)
target_link_libraries(geoframe_storage_tests PRIVATE GeoFrame::storage GTest::gtest_main)
geoframe_set_warnings(geoframe_storage_tests)

gtest_discover_tests(geoframe_storage_tests)

add_executable(
    geoframe_media_tests
    media/metadata_extractor_test.cpp
    media/preview_generator_test.cpp
    media/sha256_test.cpp
)
target_link_libraries(geoframe_media_tests PRIVATE GeoFrame::media GTest::gtest_main)
target_include_directories(geoframe_media_tests PRIVATE "${CMAKE_BINARY_DIR}")
geoframe_set_warnings(geoframe_media_tests)

gtest_discover_tests(geoframe_media_tests)

add_executable(
    geoframe_worker_tests
    worker/scan_service_test.cpp
    worker/worker_pool_test.cpp
)
target_link_libraries(
    geoframe_worker_tests
    PRIVATE
        GeoFrame::worker
        GeoFrame::db
        GTest::gtest_main
)
geoframe_set_warnings(geoframe_worker_tests)

gtest_discover_tests(geoframe_worker_tests)
