#include "database.h"
#include <sqlite3.h>
#include <iostream>

Database::Database(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
        return;
    }

    createTable();
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

void Database::createTable() {
    bool shouldRecreateItems = false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(items);", -1, &stmt, nullptr) == SQLITE_OK) {
        bool hasDescription = false;
        bool timestampIsDate = false;
        bool hasIsUnitPrice = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string columnName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string columnType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            if (columnName == "description") {
                hasDescription = true;
            }
            if (columnName == "timestamp" && columnType == "DATE") {
                timestampIsDate = true;
            }
            if (columnName == "is_unit_price") {
                hasIsUnitPrice = true;
            }
        }
        sqlite3_finalize(stmt);
        if (!hasDescription || !timestampIsDate) {
            shouldRecreateItems = true;
        }
        if (!shouldRecreateItems && !hasIsUnitPrice) {
            char* errMsg = nullptr;
            if (sqlite3_exec(db, "ALTER TABLE items ADD COLUMN is_unit_price INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                std::cerr << "SQL error while adding is_unit_price column: " << errMsg << std::endl;
                sqlite3_free(errMsg);
            }
        }
    }

    if (shouldRecreateItems) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db, "DROP TABLE IF EXISTS items;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error while dropping old items table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }

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

void Database::insertWarning(const std::string& type, const std::string& code, const std::string& description, const std::string& message) {
    const char* sql = "INSERT INTO warnings (type, code, description, message) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare warning statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute warning statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Database::insertItem(const Item& item) {
    const char* selectSql = "SELECT price FROM items WHERE code = ? AND timestamp = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare select statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, item.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, item.timestamp.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        double existingPrice = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
        if (item.price > existingPrice) {
            const char* updateSql = "UPDATE items SET price = ?, description = ?, is_unit_price = ? WHERE code = ? AND timestamp = ?;";
            if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare update statement: " << sqlite3_errmsg(db) << std::endl;
                return;
            }
            sqlite3_bind_double(stmt, 1, item.price);
            sqlite3_bind_text(stmt, 2, item.description.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, item.isUnitPrice ? 1 : 0);
            sqlite3_bind_text(stmt, 4, item.code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, item.timestamp.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute update statement: " << sqlite3_errmsg(db) << std::endl;
            } else {
                std::string warning = "Duplicate item for code " + item.code + " on " + item.timestamp + "; updated to higher price.";
                std::cerr << warning << std::endl;
                insertWarning("duplicate", item.code, item.description, warning);
            }
            sqlite3_finalize(stmt);
        } else {
            std::string warning = "Duplicate item for code " + item.code + " on " + item.timestamp + "; existing price kept.";
            std::cerr << warning << std::endl;
            insertWarning("duplicate", item.code, item.description, warning);
        }
        return;
    }
    sqlite3_finalize(stmt);

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