#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include "parser/parser.h" // for Item

class Database {
public:
    Database(const std::string& dbPath);
    ~Database();
    void createTable();
    void insertItem(const Item& item);
    std::vector<Item> queryItems(const std::string& whereClause = "");
private:
    struct sqlite3* db;
};

#endif