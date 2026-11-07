#include "server.h"
#include "guiComms/WebServer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 53000;
    std::string dbPath = "receipts.db";
    std::string webRoot = "web/dist";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--rest-port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
        } else if (arg == "--web-root" && i + 1 < argc) {
            webRoot = argv[++i];
        }
    }

    Server server(dbPath);
    if (!server.initialize()) {
        std::cerr << "Failed to initialize database." << std::endl;
        return 1;
    }

    WebServer webServer(port, server, webRoot);
    return webServer.run() ? 0 : 1;
}
