#include "db/migration_runner.hpp"

#include "db/migrations.hpp"

#include <stdexcept>
#include <string>

namespace geoframe::db {

MigrationRunner::MigrationRunner(Database& database) : database(database) {}

void MigrationRunner::migrate() {
    int current_version = database.user_version();

    for (const auto& migration : migrations) {
        if (migration.version <= current_version) {
            continue;
        }
        if (migration.version != current_version + 1) {
            throw std::runtime_error("Missing database migration after version " +
                                     std::to_string(current_version));
        }

        Transaction transaction{database};
        database.execute(migration.sql);
        database.execute("PRAGMA user_version = " + std::to_string(migration.version));
        transaction.commit();
        current_version = migration.version;
    }
}

}  // namespace geoframe::db
