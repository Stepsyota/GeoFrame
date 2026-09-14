#include "db/database.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace geoframe::db {

namespace {

[[noreturn]] void throw_sqlite_error(sqlite3* connection, std::string_view operation) {
    throw std::runtime_error(std::string{operation} + ": " + sqlite3_errmsg(connection));
}

}  // namespace

Statement::Statement(sqlite3* connection, const std::string_view sql) {
    const std::string query{sql};
    if (sqlite3_prepare_v2(connection, query.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        throw_sqlite_error(connection, "Cannot prepare SQLite statement");
    }
}

Statement::~Statement() {
    sqlite3_finalize(statement);
}

Statement::Statement(Statement&& other) noexcept : statement(std::exchange(other.statement, nullptr)) {}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(statement);
        statement = std::exchange(other.statement, nullptr);
    }
    return *this;
}

void Statement::bind(const int index, const std::int64_t value) {
    if (sqlite3_bind_int64(statement, index, value) != SQLITE_OK) {
        throw_sqlite_error(sqlite3_db_handle(statement), "Cannot bind SQLite integer");
    }
}

void Statement::bind(const int index, const double value) {
    if (sqlite3_bind_double(statement, index, value) != SQLITE_OK) {
        throw_sqlite_error(sqlite3_db_handle(statement), "Cannot bind SQLite double");
    }
}

void Statement::bind(const int index, const std::string_view value) {
    if (sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throw_sqlite_error(sqlite3_db_handle(statement), "Cannot bind SQLite text");
    }
}

void Statement::bind_null(const int index) {
    if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
        throw_sqlite_error(sqlite3_db_handle(statement), "Cannot bind SQLite null");
    }
}

bool Statement::step() {
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    throw_sqlite_error(sqlite3_db_handle(statement), "Cannot execute SQLite statement");
}

std::int64_t Statement::column_int64(const int index) const {
    return sqlite3_column_int64(statement, index);
}

double Statement::column_double(const int index) const {
    return sqlite3_column_double(statement, index);
}

std::string Statement::column_text(const int index) const {
    const auto* text = sqlite3_column_text(statement, index);
    if (text == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

bool Statement::column_is_null(const int index) const {
    return sqlite3_column_type(statement, index) == SQLITE_NULL;
}

Database::Database(const std::filesystem::path& path) {
    const std::string filename = path.string();
    constexpr int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(filename.c_str(), &connection, flags, nullptr) != SQLITE_OK) {
        const std::string error = connection == nullptr ? "unknown error" : sqlite3_errmsg(connection);
        sqlite3_close(connection);
        connection = nullptr;
        throw std::runtime_error("Cannot open SQLite database: " + error);
    }

    try {
        execute("PRAGMA foreign_keys = ON");
        execute("PRAGMA journal_mode = WAL");
        execute("PRAGMA busy_timeout = 5000");
    } catch (...) {
        sqlite3_close(connection);
        connection = nullptr;
        throw;
    }
}

Database::~Database() {
    sqlite3_close(connection);
}

Database::Database(Database&& other) noexcept : connection(std::exchange(other.connection, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close(connection);
        connection = std::exchange(other.connection, nullptr);
    }
    return *this;
}

void Database::execute(const std::string_view sql) {
    const std::string query{sql};
    char* error = nullptr;
    if (sqlite3_exec(connection, query.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error == nullptr ? sqlite3_errmsg(connection) : error;
        sqlite3_free(error);
        throw std::runtime_error("Cannot execute SQLite query: " + message);
    }
}

Statement Database::prepare(const std::string_view sql) {
    return Statement{connection, sql};
}

int Database::changes() const {
    return sqlite3_changes(connection);
}

int Database::user_version() {
    auto statement = prepare("PRAGMA user_version");
    if (!statement.step()) {
        throw std::runtime_error("SQLite did not return user_version");
    }
    return static_cast<int>(statement.column_int64(0));
}

Transaction::Transaction(Database& database) : database(database) {
    database.execute("BEGIN IMMEDIATE");
}

Transaction::~Transaction() {
    if (!committed) {
        try {
            database.execute("ROLLBACK");
        } catch (...) {
        }
    }
}

void Transaction::commit() {
    database.execute("COMMIT");
    committed = true;
}

}  // namespace geoframe::db
