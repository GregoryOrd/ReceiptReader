#include "server/server.h"
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 52000;
    std::string imgDir = "imgs";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--imgdir" && i + 1 < argc) {
            imgDir = argv[++i];
        }
    }

    std::cout << "Starting ReceiptReaderServer on port " << port << "." << std::endl;
    ReceiptReaderServer server(port, "receipts.db");
    return server.run() ? 0 : 1;
}
