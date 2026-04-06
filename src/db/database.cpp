#include "database.h"
#include <sqlite3.h>
#include <iostream>

Database::Database(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    }
}

Database::~Database() {
    sqlite3_close(db);
}

void Database::createTable() {
    const char* sql = "CREATE TABLE IF NOT EXISTS items ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "code TEXT NOT NULL,"
                      "price REAL NOT NULL,"
                      "timestamp DATETIME NOT NULL);";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

void Database::insertItem(const Item& item) {
    const char* sql = "INSERT INTO items (code, price, timestamp) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, item.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, item.price);
    sqlite3_bind_text(stmt, 3, item.timestamp.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

std::vector<Item> Database::queryItems(const std::string& whereClause) {
    std::vector<Item> items;
    std::string sql = "SELECT code, price, timestamp FROM items";
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
        item.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.price = sqlite3_column_double(stmt, 1);
        item.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
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
    std::string sql = "SELECT code, price, timestamp FROM items";
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
        item.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.price = sqlite3_column_double(stmt, 1);
        item.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}