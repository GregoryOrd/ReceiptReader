#include "database.h"
#include <sqlite3.h>
#include <iostream>

static const char* warningTypeToString(Database::WarningType type) {
    switch (type) {
        case Database::WarningType::Duplicate:
            return "duplicate";
        default:
            return "unknown";
    }
}

Database::Database(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
        return;
    }

    createTableIfNotExists();
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

void Database::createTableIfNotExists() {
    const char* sql = "CREATE TABLE IF NOT EXISTS items ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "description TEXT, "
                      "code TEXT NOT NULL, "
                      "price REAL NOT NULL, "
                      "timestamp DATE NOT NULL, "
                      "is_unit_price INTEGER NOT NULL DEFAULT 0);"
                      "CREATE UNIQUE INDEX IF NOT EXISTS idx_items_code_timestamp ON items(code, timestamp);"
                      "CREATE INDEX IF NOT EXISTS idx_items_code ON items(code);"
                      "CREATE TABLE IF NOT EXISTS warnings ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
                      "type TEXT NOT NULL, "
                      "code TEXT, "
                      "description TEXT, "
                      "message TEXT NOT NULL);";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

void Database::insertWarning(WarningType type, const std::string& code, const std::string& description, const std::string& message) {
    const char* sql = "INSERT INTO warnings (type, code, description, message) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare warning statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, warningTypeToString(type), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute warning statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Database::insertItem(const Item& item) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO items (description, code, price, timestamp, is_unit_price) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, item.description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, item.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, item.price);
    sqlite3_bind_text(stmt, 4, item.timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, item.isUnitPrice ? 1 : 0);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

std::vector<Item> Database::queryItems(const std::string& whereClause) {
    std::vector<Item> items;
    std::string sql = "SELECT description, code, price, timestamp, is_unit_price FROM items";
    if (!whereClause.empty()) {
        sql += " WHERE " + whereClause;
    }
    sql += ";";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return items;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Item item;
        item.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.price = sqlite3_column_double(stmt, 2);
        item.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        item.isUnitPrice = sqlite3_column_int(stmt, 4) != 0;
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

std::vector<Item> Database::queryItemsFiltered(const std::string& code,
                                               const std::string& priceMin,
                                               const std::string& priceMax,
                                               const std::string& dateStart,
                                               const std::string& dateEnd,
                                               bool orderByTimestamp) {
    std::vector<Item> items;
    std::string sql = "SELECT description, code, price, timestamp, is_unit_price FROM items";
    std::vector<std::string> conditions;
    std::vector<std::string> arguments;

    if (!code.empty()) {
        conditions.push_back("code LIKE ?");
        arguments.push_back("%" + code + "%");
    }
    if (!priceMin.empty()) {
        conditions.push_back("price >= ?");
        arguments.push_back(priceMin);
    }
    if (!priceMax.empty()) {
        conditions.push_back("price <= ?");
        arguments.push_back(priceMax);
    }
    if (!dateStart.empty()) {
        conditions.push_back("timestamp >= ?");
        arguments.push_back(dateStart);
    }
    if (!dateEnd.empty()) {
        conditions.push_back("timestamp <= ?");
        arguments.push_back(dateEnd);
    }

    if (!conditions.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                sql += " AND ";
            }
            sql += conditions[i];
        }
    }
    if (orderByTimestamp) {
        sql += " ORDER BY timestamp";
    }
    sql += ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return items;
    }

    for (size_t i = 0; i < arguments.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), arguments[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Item item;
        item.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.price = sqlite3_column_double(stmt, 2);
        item.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        item.isUnitPrice = sqlite3_column_int(stmt, 4) != 0;
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}