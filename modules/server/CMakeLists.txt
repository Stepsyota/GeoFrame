include(FetchContent)

# ── nlohmann/json (header-only) ────────────────────────────────────────────
find_package(nlohmann_json 3.11 CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── spdlog ─────────────────────────────────────────────────────────────────
find_package(spdlog CONFIG QUIET)
if(NOT TARGET spdlog::spdlog)
    FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# ── Boost.Beast + Boost.Asio ───────────────────────────────────────────────
find_package(Boost 1.85 COMPONENTS system QUIET)
if(NOT TARGET Boost::beast)
    message(STATUS "GeoFrame: Boost dev headers not found — fetching via FetchContent (one-time ~100 MB download)")
    set(BOOST_INCLUDE_LIBRARIES beast asio system)
    set(BOOST_ENABLE_CMAKE ON)
    FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(Boost)
endif()

find_package(OpenSSL REQUIRED)

# ── geoframe_server library ────────────────────────────────────────────────
add_library(
    geoframe_server
    STATIC
        src/server.cpp
        src/tls.cpp
        src/http_server.cpp
        src/progress_broadcaster.cpp
        src/router.cpp
        src/map_tile_service.cpp
)
add_library(GeoFrame::server ALIAS geoframe_server)

target_include_directories(
    geoframe_server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    geoframe_server
    PUBLIC
        GeoFrame::core
        GeoFrame::db
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    PRIVATE
        GeoFrame::media
        GeoFrame::worker
        Boost::beast
        Boost::asio
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
)

geoframe_set_warnings(geoframe_server)
