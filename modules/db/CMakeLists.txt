find_package(SQLite3 REQUIRED)

if(TARGET SQLite3::SQLite3)
    set(GEOFRAME_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(GEOFRAME_SQLITE_TARGET SQLite::SQLite3)
endif()

file(READ "${PROJECT_SOURCE_DIR}/migrations/001_initial.sql" GEOFRAME_MIGRATION_001)
file(READ "${PROJECT_SOURCE_DIR}/migrations/002_jobs.sql" GEOFRAME_MIGRATION_002)
file(READ "${PROJECT_SOURCE_DIR}/migrations/003_allow_duplicate_hashes.sql" GEOFRAME_MIGRATION_003)
file(READ "${PROJECT_SOURCE_DIR}/migrations/004_asset_previews.sql" GEOFRAME_MIGRATION_004)
file(READ "${PROJECT_SOURCE_DIR}/migrations/005_video_fields.sql" GEOFRAME_MIGRATION_005)
file(READ "${PROJECT_SOURCE_DIR}/migrations/006_live_photos.sql" GEOFRAME_MIGRATION_006)
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/migrations.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/db/migrations.hpp"
    @ONLY
)

add_library(
    geoframe_db
    STATIC
        src/database.cpp
        src/migration_runner.cpp
        src/sqlite_asset_repository.cpp
        src/sqlite_job_repository.cpp
)
add_library(GeoFrame::db ALIAS geoframe_db)

target_include_directories(
    geoframe_db
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(geoframe_db PUBLIC GeoFrame::core ${GEOFRAME_SQLITE_TARGET})

geoframe_set_warnings(geoframe_db)
