#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <mutex>
#include "db/database.h"
#include "processor.pb.h"

class ReceiptReaderServer {
public:
    ReceiptReaderServer(int port, const std::string& dbPath);
    bool run();

private:
    bool handleClient(int clientSock);
    bool queryItemsRequest(int clientSock, const receiptreader::QueryItemsRequest& request);
    bool processImagesRequest(int clientSock, const receiptreader::ProcessImagesRequest& request);
    bool processImagesDirectory(const std::string& receiptDir, int clientSock);
    bool sendStatus(int clientSock, bool success, const std::string& message);

    int m_port;
    Database m_db;
    std::mutex m_dbMutex;
};

#endif // SERVER_H
