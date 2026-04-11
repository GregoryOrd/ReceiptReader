#include "server.h"
#include "ProtobufServer.h"
#include "WebServer.h"
#include <atomic>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    int protoPort = 52000;
    int restPort = 53000;
    std::string dbPath = "receipts.db";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            protoPort = std::stoi(argv[++i]);
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
    WebServer webServer(restPort, server);
    std::atomic<bool> failure{false};

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

    return failure ? 1 : 0;
}
