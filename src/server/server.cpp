#include "server/server.h"
#include "processor.pb.h"
#include "ipc/message_io.h"
#include "ocr/ocr.h"
#include "parser/parser.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace fs = std::filesystem;

ReceiptReaderServer::ReceiptReaderServer(int port, const std::string& dbPath)
    : m_port(port), m_db(dbPath) {
}

bool ReceiptReaderServer::run() {
    m_db.createTableIfNotExists();

    int serverSock = createServerSocket();
    if (serverSock < 0) {
        return false;
    }

    if (!bindAndListen(serverSock)) {
        close(serverSock);
        return false;
    }

    std::cout << "ReceiptReaderServer listening on port " << m_port << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        int clientSock = acceptClient(serverSock, clientAddr);
        if (clientSock < 0) {
            continue;
        }

        logAcceptedClient(clientAddr);
        while (handleClient(clientSock)) {
            // Continue accepting multiple requests on the same client connection.
        }
        close(clientSock);
    }

    close(serverSock);
    return true;
}

int ReceiptReaderServer::createServerSocket() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "Failed to create server socket: " << strerror(errno) << std::endl;
        return -1;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return serverSock;
}

bool ReceiptReaderServer::bindAndListen(int serverSock) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

    if (bind(serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind server socket: " << strerror(errno) << std::endl;
        return false;
    }

    if (listen(serverSock, 5) < 0) {
        std::cerr << "Server failed to listen: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

int ReceiptReaderServer::acceptClient(int serverSock, sockaddr_in& clientAddr) {
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
    if (clientSock < 0) {
        std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
    }
    return clientSock;
}

void ReceiptReaderServer::logAcceptedClient(const sockaddr_in& clientAddr) {
    char clientIp[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    int clientPort = ntohs(clientAddr.sin_port);
    std::cout << "Accepted connection from " << clientIp << ":" << clientPort << std::endl;
}

bool ReceiptReaderServer::handleClient(int clientSock) {
    receiptreader::ServerRequest request;
    if (!ipc::receiveProtobufMessage(clientSock, &request)) {
        std::cerr << "Failed to receive request from client." << std::endl;
        return false;
    }

    std::cout << "Received request from client." << std::endl;
    if (request.has_query_items()) {
        return queryItemsRequest(clientSock, request.query_items());
    }
    if (request.has_process_images()) {
        std::cout << "Received process images request for directory: " << request.process_images().receipt_dir() << std::endl;
        return processImagesRequest(clientSock, request.process_images());
    }

    return sendStatus(clientSock, false, "Unsupported request type.");
}

bool ReceiptReaderServer::queryItemsRequest(int clientSock, const receiptreader::QueryItemsRequest& request) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    auto items = m_db.queryItemsFiltered(request.code(), request.price_min(), request.price_max(), request.date_start(), request.date_end(), request.order_by_timestamp());

    receiptreader::ServerResponse response;
    auto* queryResponse = response.mutable_query_items_response();
    for (const auto& item : items) {
        auto* entry = queryResponse->add_items();
        entry->set_code(item.code);
        entry->set_description(item.description);
        entry->set_price(item.price);
        entry->set_timestamp(item.timestamp);
    }

    return ipc::sendProtobufMessage(clientSock, response);
}

bool ReceiptReaderServer::processImagesRequest(int clientSock, const receiptreader::ProcessImagesRequest& request) {
    std::cout << "Processing receipt images in directory." << std::endl;
    return processImagesDirectory(request.receipt_dir(), clientSock);
}

bool ReceiptReaderServer::processImagesDirectory(const std::string& receiptDir, int clientSock) {
    fs::path directory(receiptDir);
    std::cout << "Processing receipt images in directory: " << receiptDir << std::endl;
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return sendStatus(clientSock, false, "Receipt directory does not exist: " + receiptDir);
    }

    std::vector<fs::path> imagePaths;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        const auto ext = path.extension().string();
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".HEIC" || ext == ".heic") {
            imagePaths.push_back(path);
        }
    }

    int totalImages = static_cast<int>(imagePaths.size());
    int totalItems = 0;
    int processedImages = 0;

    std::cout << "Found " << totalImages << " receipt images to process." << std::endl;
    for (const auto& path : imagePaths) {
        const auto pathString = path.string();
        std::cout << "Processing image: " << pathString << std::endl;
        std::string text = extractTextFromImage(pathString);
        std::cout << "Extracted text from image: " << text.substr(0, 100) << "..." << std::endl;
        auto items = parseReceiptText(text);

        {
            std::lock_guard<std::mutex> lock(m_dbMutex);
            for (const auto& item : items) {
                std::cout << "Inserting item into database: code=" << item.code << ", price=" << item.price << ", timestamp=" << item.timestamp << std::endl;
                m_db.insertItem(item);
                totalItems += 1;
            }
        }

        processedImages += 1;
        receiptreader::ServerResponse progressResponse;
        auto* progress = progressResponse.mutable_progress();
        progress->set_processed_images(processedImages);
        progress->set_total_images(totalImages);
        progress->set_current_image(pathString);
        if (!ipc::sendProtobufMessage(clientSock, progressResponse)) {
            return false;
        }
    }

    receiptreader::ServerResponse completeResponse;
    auto* complete = completeResponse.mutable_complete();
    complete->set_total_items(totalItems);
    complete->set_success(true);
    complete->set_message("Processing complete.");
    return ipc::sendProtobufMessage(clientSock, completeResponse);
}

bool ReceiptReaderServer::sendStatus(int clientSock, bool success, const std::string& message) {
    receiptreader::ServerResponse response;
    auto* status = response.mutable_status();
    status->set_success(success);
    status->set_message(message);
    return ipc::sendProtobufMessage(clientSock, response);
}
