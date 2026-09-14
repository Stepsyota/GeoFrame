#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace geoframe::storage {

struct ScanSummary {
    std::size_t files = 0;
    std::size_t filesystem_errors = 0;
};

/**
 * @brief Потоково обходит дерево каталогов без накопления списка файлов.
 */
class DirectoryScanner {
public:
    template <typename Callback>
    ScanSummary scan(const std::filesystem::path& root, Callback callback) const {
        if (!std::filesystem::is_directory(root)) {
            throw std::invalid_argument("Scan root is not a directory: " + root.string());
        }

        ScanSummary summary;
        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator{
            root,
            std::filesystem::directory_options::skip_permission_denied,
            error,
        };
        const std::filesystem::recursive_directory_iterator end;

        if (error) {
            ++summary.filesystem_errors;
            error.clear();
        }

        while (iterator != end) {
            const bool is_symlink = iterator->is_symlink(error);
            if (error) {
                ++summary.filesystem_errors;
                error.clear();
            } else if (!is_symlink && iterator->is_regular_file(error)) {
                callback(iterator->path());
                ++summary.files;
            }
            if (error) {
                ++summary.filesystem_errors;
                error.clear();
            }

            iterator.increment(error);
            if (error) {
                ++summary.filesystem_errors;
                error.clear();
            }
        }

        return summary;
    }
};

}  // namespace geoframe::storage
