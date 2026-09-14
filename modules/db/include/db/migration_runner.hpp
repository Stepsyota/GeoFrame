#pragma once

#include "db/database.hpp"

namespace geoframe::db {

/**
 * @brief Последовательно применяет встроенные SQL-миграции.
 */
class MigrationRunner {
public:
    explicit MigrationRunner(Database& database);

    void migrate();

private:
    Database& database;
};

}  // namespace geoframe::db
