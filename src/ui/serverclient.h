#ifndef SERVERCLIENT_H
#define SERVERCLIENT_H

#include <functional>
#include <string>
#include <vector>
#include "parser/parser.h"
#include "processor.pb.h"

class ServerClient {
public:
    ServerClient(const std::string& host = "127.0.0.1", int port = 52000);
    ~ServerClient();

    bool connectToServer();
    void disconnect();
    bool isConnected() const;
    int port() const;

    bool queryItems(const std::string& code,
                    const std::string& priceMin,
                    const std::string& priceMax,
                    const std::string& dateStart,
                    const std::string& dateEnd,
                    bool orderByTimestamp,
                    std::vector<Item>& items,
                    std::string& error);

    bool processImages(const std::string& receiptDir,
                       const std::function<void(int, int, const std::string&)>& onProgress,
                       std::string& error);

private:
    bool sendRequest(const receiptreader::ServerRequest& request, std::string& error);
    bool receiveResponse(receiptreader::ServerResponse& response, std::string& error);

    int m_sock;
    std::string m_host;
    int m_port;
};

#endif // SERVERCLIENT_H
