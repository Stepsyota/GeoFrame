#pragma once

#include "core/asset_repository.hpp"
#include "core/job_repository.hpp"
#include "storage/directory_scanner.hpp"

#include <cstddef>
#include <filesystem>

namespace geoframe::worker {

struct ScanReport {
    std::size_t files_seen = 0;
    std::size_t assets_created = 0;
    std::size_t already_indexed = 0;
    std::size_t unsupported = 0;
    std::size_t filesystem_errors = 0;
};

/**
 * @brief Оркестрирует сканирование библиотеки и постановку новых файлов в очередь.
 */
class ScanService {
public:
    ScanService(const storage::DirectoryScanner& scanner, core::IAssetRepository& assets,
                core::IJobRepository& jobs);

    ScanReport scan(const std::filesystem::path& root);

private:
    const storage::DirectoryScanner& scanner;
    core::IAssetRepository& assets;
    core::IJobRepository& jobs;
};

}  // namespace geoframe::worker
