#include "server.h"
#include "guiComms/ProtobufServer.h"
#include "guiComms/WebSocketBridge.h"
#include "guiComms/WebServer.h"
#include <QCoreApplication>
#include <QThread>
#include <atomic>
#include <future>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    int protoPort = 52000;
    int websocketPort = 52001;
    int restPort = 53000;
    std::string dbPath = "receipts.db";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            protoPort = std::stoi(argv[++i]);
        } else if (arg == "--websocket-port" && i + 1 < argc) {
            websocketPort = std::stoi(argv[++i]);
        } else if (arg == "--rest-port" && i + 1 < argc) {
            restPort = std::stoi(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
        }
    }

    Server server(dbPath);
    if (!server.initialize()) {
        std::cerr << "Failed to initialize database." << std::endl;
        return 1;
    }

    ProtobufServer protobufServer(protoPort, server);
    WebSocketBridge websocketBridge(websocketPort, protoPort);
    WebServer webServer(restPort, server);
    std::atomic<bool> failure{false};

    QThread websocketThread;
    websocketBridge.moveToThread(&websocketThread);
    std::promise<bool> bridgeStarted;
    std::future<bool> bridgeStartResult = bridgeStarted.get_future();
    QObject::connect(&websocketThread, &QThread::started, &websocketBridge,
                     [&websocketBridge, &bridgeStarted]() {
                         bridgeStarted.set_value(websocketBridge.start());
                     });
    websocketThread.start();

    if (!bridgeStartResult.get()) {
        websocketThread.quit();
        websocketThread.wait();
        return 1;
    }

    std::thread protoThread([&] {
        if (!protobufServer.run()) {
            failure = true;
        }
    });
    std::thread webThread([&] {
        if (!webServer.run()) {
            failure = true;
        }
    });

    protoThread.join();
    webThread.join();
    websocketThread.quit();
    websocketThread.wait();

    return failure ? 1 : 0;
}
