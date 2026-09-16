#include "worker/scan_service.hpp"

#include "core/asset.hpp"
#include "core/job.hpp"
#include "storage/media_file.hpp"

#include <optional>
#include <system_error>

namespace geoframe::worker {

ScanService::ScanService(const storage::DirectoryScanner& scanner, core::IAssetRepository& assets,
                         core::IJobRepository& jobs, core::ProgressTracker* progress_tracker)
    : scanner(scanner), assets(assets), jobs(jobs), progress(progress_tracker) {}

ScanReport ScanService::scan(const std::filesystem::path& root) {
    ScanReport report;
    const auto absolute_root = std::filesystem::absolute(root).lexically_normal();

    struct ScanningGuard {
        core::ProgressTracker* tracker;
        ~ScanningGuard() {
            if (tracker != nullptr) {
                tracker->set_scanning(false);
            }
        }
    } scanning_guard{progress};

    if (progress != nullptr) {
        progress->set_scanning(true);
    }

    const auto summary = scanner.scan(absolute_root, [&](const std::filesystem::path& path) {
        if (progress != nullptr) {
            progress->increment_scan_files();
            progress->set_current_file(path.filename().string());
        }
        const auto media_type = storage::detect_media_type(path);
        if (!media_type.has_value()) {
            ++report.unsupported;
            return;
        }

        const auto normalized_path = path.lexically_normal();
        const auto existing = assets.find_by_source_path(normalized_path);
        if (existing.has_value()) {
            jobs.enqueue(existing->id, core::JobType::Hash);
            ++report.already_indexed;
            return;
        }

        std::error_code error;
        const auto size = std::filesystem::file_size(normalized_path, error);
        if (error) {
            ++report.filesystem_errors;
            return;
        }

        const auto asset = assets.create(core::NewAsset{
            .source_path = normalized_path,
            .original_filename = normalized_path.filename().string(),
            .media_type = *media_type,
            .size_bytes = size,
            .sha256 = std::nullopt,
            .captured_at = std::nullopt,
            .width = std::nullopt,
            .height = std::nullopt,
            .location = std::nullopt,
            .camera = std::nullopt,
        });
        jobs.enqueue(asset.id, core::JobType::Hash);
        ++report.assets_created;
    });

    report.files_seen = summary.files;
    report.filesystem_errors += summary.filesystem_errors;
    return report;
}

}  // namespace geoframe::worker
