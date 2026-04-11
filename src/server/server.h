#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <mutex>
#include "db/database.h"
#include "processor.pb.h"

struct sockaddr_in;

class ReceiptReaderServer {
public:
    ReceiptReaderServer(int port, const std::string& dbPath);
    bool run();

private:
    int createServerSocket();
    bool bindAndListen(int serverSock);
    int acceptClient(int serverSock, sockaddr_in& clientAddr);
    void logAcceptedClient(const sockaddr_in& clientAddr);

    bool handleClient(int clientSock);
    bool queryItemsRequest(int clientSock, const receiptreader::QueryItemsRequest& request);
    bool processImagesRequest(int clientSock, const receiptreader::ProcessImagesRequest& request);
    bool processImageRequest(int clientSock, const receiptreader::ProcessImageRequest& request);
    bool confirmProcessedItemsRequest(int clientSock, const receiptreader::ConfirmProcessedItemsRequest& request);
    bool processImagesDirectory(const std::string& receiptDir, int clientSock);
    bool sendStatus(int clientSock, bool success, const std::string& message);

    int m_port;
    Database m_db;
    std::mutex m_dbMutex;
};

#endif // SERVER_H
