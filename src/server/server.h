#ifndef SERVER_H
#define SERVER_H

#include <functional>
#include <string>
#include <vector>
#include "db/database.h"
#include "parser/parser.h"
#include "processor.pb.h"

class Server {
public:
    explicit Server(const std::string& dbPath);
    bool initialize();

    std::vector<Item> queryItemsFiltered(const std::string& code,
                                         const std::string& priceMin,
                                         const std::string& priceMax,
                                         const std::string& dateStart,
                                         const std::string& dateEnd,
                                         bool orderByTimestamp);

    std::vector<Item> processImageBytes(const std::string& filename, const std::string& imageData);
    bool confirmProcessedItems(const std::vector<Item>& items);
    bool processImagesDirectory(const std::string& receiptDir,
                                const std::function<bool(const receiptreader::ProcessProgress&)>& progressCallback,
                                receiptreader::ProcessComplete& complete);

private:
    static bool writeBytesToTempFile(const std::string& filename, const std::string& data, std::string& outPath);

    Database m_db;
};

#endif // SERVER_H
