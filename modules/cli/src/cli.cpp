#include "cli/cli.hpp"

#include "core/version.hpp"

#include <iostream>
#include <string_view>

namespace geoframe::cli {

int run(const int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "GeoFrame " << core::version() << '\n';
        return 0;
    }

    std::cout << "GeoFrame " << core::version() << '\n'
              << "Usage: geoframe [--version]\n";
    return 0;
}

}  // namespace geoframe::cli
