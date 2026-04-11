#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <vector>
#include <utility>

class Server;

class WebServer {
public:
    WebServer(int port, Server& server);
    bool run();

private:
    int createServerSocket();
    bool bindAndListen(int serverSock);
    bool handleConnection(int clientSock);
    bool receiveHttpRequest(int clientSock, std::string& method, std::string& path, std::string& query, std::string& body, std::vector<std::pair<std::string, std::string>>& headers);
    void sendResponse(int clientSock, int statusCode, const std::string& statusText, const std::string& body);
    void sendJson(int clientSock, int statusCode, const std::string& body);
    std::string extractPath(const std::string& pathAndQuery, std::string& query);

    int m_port;
    Server& m_server;
};

#endif // WEBSERVER_H
