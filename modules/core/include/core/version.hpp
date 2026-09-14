#pragma once

#include <string_view>

namespace geoframe::core {

/**
 * @brief Возвращает текущую версию GeoFrame.
 */
[[nodiscard]] std::string_view version() noexcept;

}  // namespace geoframe::core
