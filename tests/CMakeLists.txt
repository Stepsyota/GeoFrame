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

add_executable(geoframe_core_tests core/version_test.cpp)
target_link_libraries(geoframe_core_tests PRIVATE GeoFrame::core GTest::gtest_main)
geoframe_set_warnings(geoframe_core_tests)

gtest_discover_tests(geoframe_core_tests)

add_executable(geoframe_db_tests db/sqlite_asset_repository_test.cpp)
target_link_libraries(geoframe_db_tests PRIVATE GeoFrame::db GTest::gtest_main)
geoframe_set_warnings(geoframe_db_tests)

gtest_discover_tests(geoframe_db_tests)
