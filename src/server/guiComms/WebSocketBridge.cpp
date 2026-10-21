#include "WebSocketBridge.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketProtocol>
#include <iostream>

WebSocketBridge::WebSocketBridge(int websocketPort, int tcpPort, QObject* parent)
    : QObject(parent),
      m_server(QStringLiteral("ReceiptReader WebSocket bridge"),
               QWebSocketServer::NonSecureMode,
               this),
      m_websocketPort(websocketPort),
      m_tcpPort(tcpPort) {
    connect(&m_server, &QWebSocketServer::newConnection,
            this, &WebSocketBridge::acceptConnections);
}

bool WebSocketBridge::start() {
    if (!m_server.listen(QHostAddress::LocalHost, static_cast<quint16>(m_websocketPort))) {
        std::cerr << "Failed to listen for WebSocket connections on port "
                  << m_websocketPort << ": "
                  << m_server.errorString().toStdString() << std::endl;
        return false;
    }

    std::cout << "WebSocket bridge listening on port " << m_websocketPort
              << " and forwarding to TCP port " << m_tcpPort << std::endl;
    return true;
}

void WebSocketBridge::acceptConnections() {
    while (m_server.hasPendingConnections()) {
        handleConnection(m_server.nextPendingConnection());
    }
}

void WebSocketBridge::handleConnection(QWebSocket* webSocket) {
    auto* tcpSocket = new QTcpSocket(webSocket);

    connect(webSocket, &QWebSocket::binaryMessageReceived,
            tcpSocket, [tcpSocket](const QByteArray& message) {
                if (tcpSocket->state() == QAbstractSocket::UnconnectedState) {
                    return;
                }
                if (tcpSocket->write(message) == -1) {
                    tcpSocket->abort();
                }
            });

    connect(webSocket, &QWebSocket::textMessageReceived,
            webSocket, [webSocket](const QString&) {
                webSocket->close(QWebSocketProtocol::CloseCodeDatatypeNotSupported,
                                 QStringLiteral("Only binary protobuf data is supported"));
            });

    connect(tcpSocket, &QTcpSocket::connected, webSocket, [webSocket]() {
        std::cout << "WebSocket client connected to TCP protobuf server" << std::endl;
    });

    connect(tcpSocket, &QTcpSocket::readyRead, webSocket, [tcpSocket, webSocket]() {
        const QByteArray data = tcpSocket->readAll();
        if (webSocket->state() == QAbstractSocket::ConnectedState
            && webSocket->sendBinaryMessage(data) == -1) {
            webSocket->close(QWebSocketProtocol::CloseCodeBadOperation,
                             QStringLiteral("Failed to forward TCP response"));
        }
    });

    connect(webSocket, &QWebSocket::disconnected, tcpSocket, [tcpSocket, webSocket]() {
        tcpSocket->abort();
        webSocket->deleteLater();
    });

    connect(tcpSocket, &QTcpSocket::disconnected, webSocket, [webSocket]() {
        if (webSocket->state() == QAbstractSocket::ConnectedState) {
            webSocket->close(QWebSocketProtocol::CloseCodeGoingAway,
                             QStringLiteral("TCP protobuf connection closed"));
        }
    });

    connect(tcpSocket, &QTcpSocket::errorOccurred, webSocket,
            [webSocket](QAbstractSocket::SocketError) {
                if (webSocket->state() == QAbstractSocket::ConnectedState) {
                    webSocket->close(QWebSocketProtocol::CloseCodeBadOperation,
                                     QStringLiteral("Could not connect to TCP protobuf server"));
                }
            });

    tcpSocket->connectToHost(QHostAddress::LocalHost, static_cast<quint16>(m_tcpPort));
}
