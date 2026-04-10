#include "ui/serverclient.h"
#include "processor.pb.h"
#include "ipc/message_io.h"
#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

ServerClient::ServerClient(const std::string& host, int port)
    : m_sock(-1), m_host(host), m_port(port) {
}

ServerClient::~ServerClient() {
    disconnect();
}

bool ServerClient::connectToServer() {
    if (isConnected()) {
        return true;
    }

    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr) <= 0) {
        disconnect();
        return false;
    }

    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        disconnect();
        return false;
    }

    return true;
}

void ServerClient::disconnect() {
    if (isConnected()) {
        close(m_sock);
        m_sock = -1;
    }
}

bool ServerClient::isConnected() const {
    return m_sock >= 0;
}

bool ServerClient::sendRequest(const receiptreader::ServerRequest& request, std::string& error) {
    if (!isConnected()) {
        error = "Not connected to server.";
        return false;
    }

    if (!ipc::sendProtobufMessage(m_sock, request)) {
        error = "Failed to send request to server.";
        return false;
    }
    return true;
}

bool ServerClient::receiveResponse(receiptreader::ServerResponse& response, std::string& error) {
    if (!isConnected()) {
        error = "Not connected to server.";
        return false;
    }

    if (!ipc::receiveProtobufMessage(m_sock, &response)) {
        error = "Failed to receive response from server.";
        return false;
    }
    return true;
}

bool ServerClient::queryItems(const std::string& code,
                              const std::string& priceMin,
                              const std::string& priceMax,
                              const std::string& dateStart,
                              const std::string& dateEnd,
                              bool orderByTimestamp,
                              std::vector<Item>& items,
                              std::string& error) {
    receiptreader::ServerRequest request;
    auto* query = request.mutable_query_items();
    query->set_code(code);
    query->set_price_min(priceMin);
    query->set_price_max(priceMax);
    query->set_date_start(dateStart);
    query->set_date_end(dateEnd);
    query->set_order_by_timestamp(orderByTimestamp);

    if (!sendRequest(request, error)) {
        return false;
    }

    receiptreader::ServerResponse response;
    if (!receiveResponse(response, error)) {
        return false;
    }

    if (response.has_status()) {
        error = response.status().message();
        return false;
    }
    if (!response.has_query_items_response()) {
        error = "Unexpected server response.";
        return false;
    }

    items.clear();
    for (const auto& entry : response.query_items_response().items()) {
        Item item;
        item.description = entry.description();
        item.code = entry.code();
        item.price = entry.price();
        item.timestamp = entry.timestamp();
        items.push_back(item);
    }
    return true;
}

bool ServerClient::processImages(const std::string& receiptDir,
                                 const std::function<void(int, int, const std::string&)>& onProgress,
                                 std::string& error) {
    receiptreader::ServerRequest request;
    auto* process = request.mutable_process_images();
    process->set_receipt_dir(receiptDir);

    if (!sendRequest(request, error)) {
        return false;
    }

    while (true) {
        receiptreader::ServerResponse response;
        if (!receiveResponse(response, error)) {
            return false;
        }

        if (response.has_status()) {
            if (!response.status().success()) {
                error = response.status().message();
                return false;
            }
            return true;
        }

        if (response.has_progress()) {
            if (onProgress) {
                onProgress(response.progress().processed_images(), response.progress().total_images(), response.progress().current_image());
            }
            continue;
        }

        if (response.has_complete()) {
            if (!response.complete().success()) {
                error = response.complete().message();
                return false;
            }
            return true;
        }

        error = "Unexpected server response during processing.";
        return false;
    }
}
