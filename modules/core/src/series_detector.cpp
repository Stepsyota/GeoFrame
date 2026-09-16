#include "core/series_detector.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace geoframe::core {

namespace {

std::string normalize_captured_at(const std::string& captured_at) {
    if (captured_at.size() < 19) {
        return {};
    }

    std::string normalized = captured_at.substr(0, 19);
    if (normalized[4] == ':' && normalized[7] == ':' && normalized[10] == ' ') {
        normalized[4] = '-';
        normalized[7] = '-';
        normalized[10] = 'T';
    }

    if (normalized[4] != '-' || normalized[7] != '-' || normalized[10] != 'T'
        || normalized[13] != ':' || normalized[16] != ':') {
        return {};
    }

    return normalized;
}

}  // namespace

std::optional<std::int64_t> parse_captured_epoch(const std::string& captured_at) {
    const auto normalized = normalize_captured_at(captured_at);
    if (normalized.empty()) {
        return std::nullopt;
    }

    std::tm tm{};
    std::istringstream stream(normalized);
    stream >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (stream.fail()) {
        return std::nullopt;
    }

    tm.tm_isdst = -1;
    const auto epoch = timegm(&tm);
    if (epoch < 0) {
        return std::nullopt;
    }

    return static_cast<std::int64_t>(epoch);
}

std::vector<PhotoSeries> detect_photo_series(const std::vector<TimedAsset>& assets,
                                             const int max_gap_seconds) {
    if (assets.empty() || max_gap_seconds < 0) {
        return {};
    }

    struct ParsedAsset {
        std::int64_t id;
        std::int64_t epoch;
    };

    std::vector<ParsedAsset> parsed;
    parsed.reserve(assets.size());
    for (const auto& asset : assets) {
        const auto epoch = parse_captured_epoch(asset.captured_at);
        if (!epoch.has_value()) {
            continue;
        }
        parsed.push_back({.id = asset.id, .epoch = *epoch});
    }

    std::sort(parsed.begin(), parsed.end(),
              [](const ParsedAsset& left, const ParsedAsset& right) {
                  if (left.epoch != right.epoch) {
                      return left.epoch < right.epoch;
                  }
                  return left.id < right.id;
              });

    std::vector<PhotoSeries> series;
    std::vector<std::int64_t> current;
    std::optional<std::int64_t> previous_epoch;

    for (const auto& asset : parsed) {
        if (!previous_epoch.has_value()
            || asset.epoch - *previous_epoch > max_gap_seconds) {
            if (current.size() >= 2) {
                series.push_back(PhotoSeries{.asset_ids = current});
            }
            current = {asset.id};
        } else {
            current.push_back(asset.id);
        }
        previous_epoch = asset.epoch;
    }

    if (current.size() >= 2) {
        series.push_back(PhotoSeries{.asset_ids = current});
    }

    std::sort(series.begin(), series.end(),
              [](const PhotoSeries& left, const PhotoSeries& right) {
                  return left.asset_ids.size() > right.asset_ids.size();
              });

    return series;
}

}  // namespace geoframe::core
