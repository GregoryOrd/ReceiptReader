#ifndef DATABASE_H
#define DATABASE_H

#include <mutex>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "parser/parser.h" // for Item

class Database {
public:
    enum class WarningType {
        Duplicate,
        Unknown
    };

    Database(const std::string& dbPath);
    ~Database();
    bool createTableIfNotExists();
    void insertItem(const Item& item);
    void insertWarning(WarningType type, const std::string& code, const std::string& description, const std::string& message);
    std::vector<Item> queryItems(const std::string& whereClause = "");
    std::vector<Item> queryItemsFiltered(const std::string& code,
                                         const std::string& priceMin,
                                         const std::string& priceMax,
                                         const std::string& dateStart,
                                         const std::string& dateEnd,
                                         bool orderByTimestamp = false);
private:
    bool prepareStatement(const char* sql, sqlite3_stmt** stmt);

    struct sqlite3* db;
    sqlite3_stmt* updateItemStmt = nullptr;
    sqlite3_stmt* insertItemStmt = nullptr;
    sqlite3_stmt* insertWarningStmt = nullptr;
    std::mutex m_mutex;
};

#endif