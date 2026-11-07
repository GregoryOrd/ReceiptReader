#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QJsonObject>

#include <map>
#include <memory>
#include <mutex>
#include <string>

class Server;

class WebServer {
public:
    struct BatchJob {
        std::string id;
        std::string status;
        std::string currentImage;
        std::string message;
        int processedImages = 0;
        int totalImages = 0;
        int totalItems = 0;
        bool success = false;
    };

    WebServer(int port, Server& server, std::string webRoot = "web/dist");
    bool run();

private:
    int createServerSocket();
    bool bindAndListen(int serverSock);
    bool handleConnection(int clientSock);
    bool serveStatic(int clientSock, const std::string& path);
    void sendResponse(int clientSock,
                      int statusCode,
                      const std::string& status,
                      const std::string& body,
                      const std::string& contentType = "application/json; charset=utf-8",
                      bool cors = false,
                      const std::string& location = {});
    void sendJson(int clientSock, int statusCode, const QJsonObject& body);

    int m_port;
    Server& m_server;
    std::string m_webRoot;
    std::mutex m_jobsMutex;
    std::map<std::string, std::shared_ptr<BatchJob>> m_batchJobs;
};

#endif // WEBSERVER_H
