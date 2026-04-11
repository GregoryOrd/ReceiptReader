#include "ProtobufServer.h"
#include "ipc/message_io.h"
#include "processor.pb.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ProtobufServer::ProtobufServer(int port, Server& server)
    : m_port(port), m_server(server) {
}

bool ProtobufServer::run() {
    int serverSock = createServerSocket();
    if (serverSock < 0) {
        return false;
    }

    if (!bindAndListen(serverSock)) {
        close(serverSock);
        return false;
    }

    std::cout << "Protobuf server listening on port " << m_port << std::endl;

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

int ProtobufServer::createServerSocket() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "Failed to create server socket: " << strerror(errno) << std::endl;
        return -1;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return serverSock;
}

bool ProtobufServer::bindAndListen(int serverSock) {
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

int ProtobufServer::acceptClient(int serverSock, sockaddr_in& clientAddr) {
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
    if (clientSock < 0) {
        std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
    }
    return clientSock;
}

void ProtobufServer::logAcceptedClient(const sockaddr_in& clientAddr) {
    char clientIp[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    int clientPort = ntohs(clientAddr.sin_port);
    std::cout << "Accepted protobuf connection from " << clientIp << ":" << clientPort << std::endl;
}

bool ProtobufServer::handleClient(int clientSock) {
    receiptreader::ServerRequest request;
    if (!ipc::receiveProtobufMessage(clientSock, &request)) {
        std::cerr << "Failed to receive request from protobuf client." << std::endl;
        return false;
    }

    if (request.has_query_items()) {
        return queryItemsRequest(clientSock, request.query_items());
    }
    if (request.has_process_images()) {
        return processImagesRequest(clientSock, request.process_images());
    }
    if (request.has_process_image()) {
        return processImageRequest(clientSock, request.process_image());
    }
    if (request.has_confirm_processed_items()) {
        return confirmProcessedItemsRequest(clientSock, request.confirm_processed_items());
    }

    return sendStatus(clientSock, false, "Unsupported request type.");
}

bool ProtobufServer::queryItemsRequest(int clientSock, const receiptreader::QueryItemsRequest& request) {
    auto items = m_server.queryItemsFiltered(request.code(), request.price_min(), request.price_max(), request.date_start(), request.date_end(), request.order_by_timestamp());

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

bool ProtobufServer::processImagesRequest(int clientSock, const receiptreader::ProcessImagesRequest& request) {
    receiptreader::ProcessComplete complete;
    bool success = m_server.processImagesDirectory(request.receipt_dir(), [&](const receiptreader::ProcessProgress& progress) {
        receiptreader::ServerResponse progressResponse;
        *progressResponse.mutable_progress() = progress;
        return ipc::sendProtobufMessage(clientSock, progressResponse);
    }, complete);

    if (!success) {
        receiptreader::ServerResponse response;
        *response.mutable_complete() = complete;
        return ipc::sendProtobufMessage(clientSock, response);
    }

    receiptreader::ServerResponse response;
    *response.mutable_complete() = complete;
    return ipc::sendProtobufMessage(clientSock, response);
}

bool ProtobufServer::processImageRequest(int clientSock, const receiptreader::ProcessImageRequest& request) {
    auto items = m_server.processImageBytes(request.filename(), request.image_data());
    receiptreader::ServerResponse response;
    auto* imageResponse = response.mutable_process_image_response();
    for (const auto& item : items) {
        auto* entry = imageResponse->add_items();
        entry->set_code(item.code);
        entry->set_description(item.description);
        entry->set_price(item.price);
        entry->set_timestamp(item.timestamp);
    }
    return ipc::sendProtobufMessage(clientSock, response);
}

bool ProtobufServer::confirmProcessedItemsRequest(int clientSock, const receiptreader::ConfirmProcessedItemsRequest& request) {
    std::vector<Item> items;
    for (const auto& entry : request.items()) {
        Item item;
        item.code = entry.code();
        item.description = entry.description();
        item.price = entry.price();
        item.timestamp = entry.timestamp();
        item.isUnitPrice = false;
        items.push_back(item);
    }

    bool success = m_server.confirmProcessedItems(items);
    return sendStatus(clientSock, success, success ? "Items saved to database." : "Failed to save items.");
}

bool ProtobufServer::sendStatus(int clientSock, bool success, const std::string& message) {
    receiptreader::ServerResponse response;
    auto* status = response.mutable_status();
    status->set_success(success);
    status->set_message(message);
    return ipc::sendProtobufMessage(clientSock, response);
}
