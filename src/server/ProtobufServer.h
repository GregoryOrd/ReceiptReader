#ifndef PROTOBUFSERVER_H
#define PROTOBUFSERVER_H

#include <string>
#include "server.h"

class ProtobufServer {
public:
    ProtobufServer(int port, Server& server);
    bool run();

private:
    int createServerSocket();
    bool bindAndListen(int serverSock);
    int acceptClient(int serverSock, struct sockaddr_in& clientAddr);
    void logAcceptedClient(const struct sockaddr_in& clientAddr);
    bool handleClient(int clientSock);
    bool queryItemsRequest(int clientSock, const receiptreader::QueryItemsRequest& request);
    bool processImagesRequest(int clientSock, const receiptreader::ProcessImagesRequest& request);
    bool processImageRequest(int clientSock, const receiptreader::ProcessImageRequest& request);
    bool confirmProcessedItemsRequest(int clientSock, const receiptreader::ConfirmProcessedItemsRequest& request);
    bool sendStatus(int clientSock, bool success, const std::string& message);

    int m_port;
    Server& m_server;
};

#endif // PROTOBUFSERVER_H
