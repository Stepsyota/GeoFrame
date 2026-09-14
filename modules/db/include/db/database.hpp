#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace geoframe::db {

/**
 * @brief Подготовленный SQLite statement.
 *
 * Класс владеет sqlite3_stmt и освобождает его через sqlite3_finalize.
 */
class Statement {
public:
    Statement(sqlite3* connection, std::string_view sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void bind(int index, std::int64_t value);
    void bind(int index, double value);
    void bind(int index, std::string_view value);
    void bind_null(int index);

    /**
     * @return true, если доступна строка; false, если выполнение завершено.
     */
    bool step();

    [[nodiscard]] std::int64_t column_int64(int index) const;
    [[nodiscard]] double column_double(int index) const;
    [[nodiscard]] std::string column_text(int index) const;
    [[nodiscard]] bool column_is_null(int index) const;

private:
    sqlite3_stmt* statement = nullptr;
};

/**
 * @brief RAII-обёртка над SQLite connection.
 */
class Database {
public:
    explicit Database(const std::filesystem::path& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    void execute(std::string_view sql);
    [[nodiscard]] Statement prepare(std::string_view sql);
    [[nodiscard]] int changes() const;
    [[nodiscard]] int user_version();

private:
    sqlite3* connection = nullptr;
};

/**
 * @brief Транзакция с автоматическим rollback, если commit не был вызван.
 */
class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();

private:
    Database& database;
    bool committed = false;
};

}  // namespace geoframe::db
