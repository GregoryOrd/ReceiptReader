#ifndef WEBSOCKETBRIDGE_H
#define WEBSOCKETBRIDGE_H

#include <QObject>
#include <QWebSocketServer>

class WebSocketBridge : public QObject {
public:
    WebSocketBridge(int websocketPort, int tcpPort, QObject* parent = nullptr);
    bool start();

private:
    void acceptConnections();
    void handleConnection(QWebSocket* webSocket);

    QWebSocketServer m_server;
    int m_websocketPort;
    int m_tcpPort;
};

#endif // WEBSOCKETBRIDGE_H
