#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace geoframe::server {

/**
 * @brief Map basemap storage: single PMTiles region file, with Carto tile proxy fallback.
 */
class MapTileService {
public:
    static constexpr int kUpstreamMaxZoom = 14;
    /** Default offline zoom for `map download world` (~15–20 GB). */
    static constexpr int kDefaultWorldMaxZoom = 13;
    static constexpr const char* kRegionFileName = "region.pmtiles";

    explicit MapTileService(const std::filesystem::path& data_dir);

    /** Zoom through which the local PMTiles archive is used; above → remote Carto tiles. */
    [[nodiscard]] int offline_max_zoom() const;

    /** Large download/extract staging dir (default: `<data-dir>/tmp`, override: `GEOFRAME_TMP_DIR`). */
    [[nodiscard]] std::filesystem::path staging_dir() const;
    [[nodiscard]] std::filesystem::path region_path() const;
    [[nodiscard]] bool has_region() const;
    [[nodiscard]] std::uint64_t region_bytes() const;
    /** Max zoom stored in the local PMTiles file (hybrid mode). */
    [[nodiscard]] int region_max_zoom() const;

    /** Stream-download a PMTiles archive to region_path(). Resumes partial .part files. */
    bool download_region(const std::string& url,
                         const std::function<void(std::uint64_t downloaded,
                                                  std::uint64_t total)>& on_progress);

    /** Latest Protomaps daily planet build URL (z0–15, ~120 GB). */
    [[nodiscard]] static std::string latest_protomaps_planet_url();

    /**
     * Extract world tiles z0…max_zoom from a remote planet archive via the `pmtiles` CLI.
     * Auto-installs the tool into <data-dir>/bin/ when missing.
     */
    using OutputFn = std::function<void(std::string_view)>;

    bool extract_world_subset(const std::string& source_url, int max_zoom,
                              const OutputFn& output = {}, bool dry_run_only = false);

    /** Download go-pmtiles into <data-dir>/bin/pmtiles if not already available. */
    bool ensure_pmtiles_tool(const OutputFn& output = {}) const;

    void save_region_config(int local_max_zoom, bool hybrid) const;

    /** Returns a path to a tile file, fetching from upstream when missing (Carto fallback). */
    [[nodiscard]] std::optional<std::filesystem::path> tile(int z, int x, int y);

    /** Font glyph ranges used for map labels. */
    [[nodiscard]] std::optional<std::filesystem::path> font(const std::string& fontstack,
                                                            const std::string& range);

    /** Sprite sheet assets (sprite.json / sprite.png). */
    [[nodiscard]] std::optional<std::filesystem::path> sprite(const std::string& name);

    [[nodiscard]] std::uint64_t archive_bytes() const;
    [[nodiscard]] std::uint64_t cache_bytes() const;
    [[nodiscard]] std::uint64_t archive_tile_count() const;
    [[nodiscard]] std::uint64_t cache_tile_count() const;

private:
    [[nodiscard]] std::filesystem::path tile_path(int z, int x, int y, bool archive) const;
    [[nodiscard]] static bool valid_tile_coords(int z, int x, int y);
    [[nodiscard]] static std::string upstream_tile_host(int x, int y);
    [[nodiscard]] bool fetch_https(const std::string& host, const std::string& target_path,
                                   const std::filesystem::path& dest) const;
    [[nodiscard]] bool fetch_https_to_file(
        const std::string& host, const std::string& target_path,
        const std::filesystem::path& dest,
        const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const;
    [[nodiscard]] static bool https_head_ok(const std::string& host, const std::string& target_path,
                                            std::uint64_t* content_length = nullptr);
    [[nodiscard]] std::filesystem::path config_path() const;
    [[nodiscard]] std::filesystem::path bundled_pmtiles_path() const;
    [[nodiscard]] std::optional<std::filesystem::path> find_pmtiles_executable() const;
    [[nodiscard]] std::filesystem::path region_staging_path() const;

    std::filesystem::path tmp_dir_;
    std::filesystem::path bin_dir_;
    std::filesystem::path map_dir_;
    std::filesystem::path archive_dir_;
    std::filesystem::path cache_dir_;
    std::filesystem::path font_cache_dir_;
    std::filesystem::path sprite_cache_dir_;
};

}  // namespace geoframe::server
