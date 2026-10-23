#include "ui/serverConn/serverclient.h"
#include "proto/processor.pb.h"
#include "ipc/message_io.h"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#ifdef BUILD_WASM
#include <QTimer>
#endif

#ifdef BUILD_WASM
namespace {

using ResponseCompletion =
    std::function<void(bool, receiptreaderproto::ServerResponse, std::string)>;

struct PendingResponse {
    std::vector<char> bytes;
    std::size_t totalSize = 0;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(30);
};

std::vector<PriceHistorySummary> queryItemsFromResponse(
    const receiptreaderproto::ServerResponse& response) {
    std::vector<PriceHistorySummary> items;
    for (const auto& entry : response.query_items_response().items()) {
        items.push_back(PriceHistorySummary(
            entry.description(),
            entry.code(),
            entry.timestampfirst(),
            entry.timestamplast(),
            entry.liftimeinflationrate(),
            entry.oneyearinflationrate(),
            entry.sixmonthinflationrate(),
            entry.threemonthinflationrate(),
            entry.onemonthinflationrate(),
            entry.latestpricechange(),
            entry.timespurchased(),
            entry.minprice(),
            entry.maxprice(),
            entry.currentprice(),
            entry.isunitprice()));
    }
    return items;
}

void pollResponse(int sock,
                  QObject* context,
                  const std::shared_ptr<PendingResponse>& pending,
                  const ResponseCompletion& completion) {
    char chunk[4096];
    while (true) {
        const ssize_t received = recv(sock, chunk, sizeof(chunk), MSG_DONTWAIT);
        if (received > 0) {
            pending->bytes.insert(pending->bytes.end(), chunk, chunk + received);
            if (pending->totalSize == 0 && pending->bytes.size() >= sizeof(uint32_t)) {
                uint32_t networkLength;
                std::memcpy(&networkLength, pending->bytes.data(), sizeof(networkLength));
                const uint32_t payloadLength = ntohl(networkLength);
                if (payloadLength > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
                    completion(false, {}, "Protobuf response is too large.");
                    return;
                }
                pending->totalSize = sizeof(networkLength) + payloadLength;
            }
            if (pending->totalSize > 0 && pending->bytes.size() >= pending->totalSize) {
                receiptreaderproto::ServerResponse response;
                const auto payloadLength = static_cast<int>(pending->totalSize - sizeof(uint32_t));
                if (!response.ParseFromArray(pending->bytes.data() + sizeof(uint32_t), payloadLength)) {
                    completion(false, {}, "Failed to parse protobuf response.");
                } else {
                    completion(true, std::move(response), {});
                }
                return;
            }
            continue;
        }

        if (received == 0) {
            completion(false, {}, "Connection closed before the server response arrived.");
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            completion(false, {}, std::string("Failed to receive protobuf response: ")
                                      + strerror(errno) + " (errno "
                                      + std::to_string(errno) + ").");
            return;
        }
        break;
    }

    if (std::chrono::steady_clock::now() >= pending->deadline) {
        completion(false, {}, "Timed out waiting for the server response.");
        return;
    }

    QTimer::singleShot(10, context, [sock, context, pending, completion]() {
        pollResponse(sock, context, pending, completion);
    });
}

template <typename Entries>
std::vector<Item> itemsFromResponse(const Entries& entries) {
    std::vector<Item> items;
    for (const auto& entry : entries) {
        Item item;
        item.description = entry.description();
        item.code = entry.code();
        item.price = entry.price();
        item.timestamp = entry.timestamp();
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace
#endif

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

int ServerClient::port() const {
    return m_port;
}

bool ServerClient::sendRequest(const receiptreaderproto::ServerRequest& request, std::string& error) const {
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

bool ServerClient::receiveResponse(receiptreaderproto::ServerResponse& response, std::string& error) const {
    if (!isConnected()) {
        error = "Not connected to server.";
        return false;
    }

    if (!ipc::receiveProtobufMessage(m_sock, &response, error)) {
        return false;
    }
    return true;
}

bool ServerClient::queryItems(const std::string& code,
                              const std::string& desc,
                              const std::string& priceMin,
                              const std::string& priceMax,
                              const std::string& dateStart,
                              const std::string& dateEnd,
                              std::vector<PriceHistorySummary>& items,
                              std::string& error) const {
    receiptreaderproto::ServerRequest request;
    auto* query = request.mutable_query_items();
    query->set_code(code);
    query->set_desc(desc);
    query->set_price_min(priceMin);
    query->set_price_max(priceMax);
    query->set_date_start(dateStart);
    query->set_date_end(dateEnd);

    if (!sendRequest(request, error)) {
        return false;
    }

    std::wcout << "Receving Response" << std::endl;
    receiptreaderproto::ServerResponse response;
    if (!receiveResponse(response, error)) {
        return false;
    }
    std::wcout << "Response Had Error" << std::endl;

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

        std::cout << "Found time purchased: " << entry.timespurchased() << " for code: " << entry.code() << " with current price: " << entry.currentprice() << std::endl;

        items.push_back(PriceHistorySummary(
            entry.description(),
            entry.code(),
            entry.timestampfirst(),
            entry.timestamplast(),
            entry.liftimeinflationrate(),
            entry.oneyearinflationrate(),
            entry.sixmonthinflationrate(),
            entry.threemonthinflationrate(),
            entry.onemonthinflationrate(),
            entry.latestpricechange(),
            entry.timespurchased(),
            entry.minprice(),
            entry.maxprice(),
            entry.currentprice(),
            entry.isunitprice()
        ));
    }
    return true;
}

#ifdef BUILD_WASM
void ServerClient::queryItemsAsync(
    const std::string& code,
    const std::string& desc,
    const std::string& priceMin,
    const std::string& priceMax,
    const std::string& dateStart,
    const std::string& dateEnd,
    QObject* context,
    std::function<void(bool, std::vector<PriceHistorySummary>, std::string)> completion) const {
    if (!context) {
        completion(false, {}, "Missing context for asynchronous query.");
        return;
    }

    receiptreaderproto::ServerRequest request;
    auto* query = request.mutable_query_items();
    query->set_code(code);
    query->set_desc(desc);
    query->set_price_min(priceMin);
    query->set_price_max(priceMax);
    query->set_date_start(dateStart);
    query->set_date_end(dateEnd);

    std::string error;
    if (!sendRequest(request, error)) {
        completion(false, {}, std::move(error));
        return;
    }

    auto pending = std::make_shared<PendingResponse>();
    QTimer::singleShot(0, context, [this, context, pending, completion = std::move(completion)]() {
        pollResponse(m_sock, context, pending,
                     [completion](bool success, receiptreaderproto::ServerResponse response,
                                  std::string error) mutable {
                         if (!success) {
                             completion(false, {}, std::move(error));
                         } else if (response.has_status()) {
                             completion(false, {}, response.status().message());
                         } else if (!response.has_query_items_response()) {
                             completion(false, {}, "Unexpected server response.");
                         } else {
                             completion(true, queryItemsFromResponse(response), {});
                         }
                     });
    });
}

void ServerClient::sendRequestAsync(
    const receiptreaderproto::ServerRequest& request,
    QObject* context,
    std::function<void(bool, receiptreaderproto::ServerResponse, std::string)> completion) const {
    if (!context) {
        completion(false, {}, "Missing context for asynchronous request.");
        return;
    }

    std::string error;
    if (!sendRequest(request, error)) {
        completion(false, {}, std::move(error));
        return;
    }

    auto pending = std::make_shared<PendingResponse>();
    QTimer::singleShot(0, context, [this, context, pending, completion = std::move(completion)]() {
        pollResponse(m_sock, context, pending, completion);
    });
}

void ServerClient::queryItemCodeAsync(
    const std::string& code,
    QObject* context,
    std::function<void(bool, std::vector<Item>, std::string)> completion) const {
    receiptreaderproto::ServerRequest request;
    request.mutable_query_code()->set_code(code);
    sendRequestAsync(request, context,
                     [completion = std::move(completion)](
                         bool success, receiptreaderproto::ServerResponse response,
                         std::string error) mutable {
                         if (!success) {
                             completion(false, {}, std::move(error));
                         } else if (response.has_status()) {
                             completion(false, {}, response.status().message());
                         } else if (!response.has_query_item_code_response()) {
                             completion(false, {}, "Unexpected server response.");
                         } else {
                             completion(true,
                                        itemsFromResponse(response.query_item_code_response().items()),
                                        {});
                         }
                     });
}

void ServerClient::processImageAsync(
    const std::vector<uint8_t>& imageData,
    const std::string& filename,
    QObject* context,
    std::function<void(bool, std::vector<Item>, std::string)> completion) const {
    receiptreaderproto::ServerRequest request;
    auto* imageRequest = request.mutable_process_image();
    imageRequest->set_image_data(reinterpret_cast<const char*>(imageData.data()),
                                 static_cast<int>(imageData.size()));
    if (!filename.empty()) {
        imageRequest->set_filename(filename);
    }
    sendRequestAsync(request, context,
                     [completion = std::move(completion)](
                         bool success, receiptreaderproto::ServerResponse response,
                         std::string error) mutable {
                         if (!success) {
                             completion(false, {}, std::move(error));
                         } else if (response.has_status()) {
                             completion(false, {}, response.status().message());
                         } else if (!response.has_process_image_response()) {
                             completion(false, {}, "Unexpected server response.");
                         } else {
                             completion(true,
                                        itemsFromResponse(response.process_image_response().items()),
                                        {});
                         }
                     });
}

void ServerClient::confirmProcessedItemsAsync(
    const std::vector<Item>& items,
    const std::string& date,
    QObject* context,
    std::function<void(bool, std::string)> completion) const {
    receiptreaderproto::ServerRequest request;
    auto* confirmRequest = request.mutable_confirm_processed_items();
    confirmRequest->set_date(date);
    for (const auto& item : items) {
        auto* entry = confirmRequest->add_items();
        entry->set_code(item.code);
        entry->set_description(item.description);
        entry->set_price(item.price);
        entry->set_timestamp(item.timestamp);
    }
    sendRequestAsync(request, context,
                     [completion = std::move(completion)](
                         bool success, receiptreaderproto::ServerResponse response,
                         std::string error) mutable {
                         if (!success) {
                             completion(false, std::move(error));
                         } else if (!response.has_status()) {
                             completion(false, "Unexpected server response.");
                         } else if (!response.status().success()) {
                             completion(false, response.status().message());
                         } else {
                             completion(true, {});
                         }
                     });
}
#endif

bool ServerClient::queryItemCode(const std::string& code,
                              std::vector<Item>& items,
                              std::string& error) const {
    receiptreaderproto::ServerRequest request;
    auto* query = request.mutable_query_code();
    query->set_code(code);

    if (!sendRequest(request, error)) {
        return false;
    }

    receiptreaderproto::ServerResponse response;
    if (!receiveResponse(response, error)) {
        return false;
    }

    if (response.has_status()) {
        error = response.status().message();
        return false;
    }
    if (!response.has_query_item_code_response()) {
        error = "Unexpected server response.";
        return false;
    }

    items.clear();
    for (const auto& entry : response.query_item_code_response().items()) {
        Item item;
        item.description = entry.description();
        item.code = entry.code();
        item.price = entry.price();
        item.timestamp = entry.timestamp();
        items.push_back(item);
    }
    return true;
}

bool ServerClient::processImage(const std::vector<uint8_t>& imageData,
                                const std::string& filename,
                                std::vector<Item>& items,
                                std::string& error) const {
    receiptreaderproto::ServerRequest request;
    auto* imageRequest = request.mutable_process_image();
    imageRequest->set_image_data(reinterpret_cast<const char*>(imageData.data()), static_cast<int>(imageData.size()));
    if (!filename.empty()) {
        imageRequest->set_filename(filename);
    }

    if (!sendRequest(request, error)) {
        return false;
    }

    receiptreaderproto::ServerResponse response;
    if (!receiveResponse(response, error)) {
        return false;
    }

    if (response.has_status()) {
        error = response.status().message();
        return false;
    }
    if (!response.has_process_image_response()) {
        error = "Unexpected server response.";
        return false;
    }

    items.clear();
    for (const auto& entry : response.process_image_response().items()) {
        Item item;
        item.description = entry.description();
        item.code = entry.code();
        item.price = entry.price();
        item.timestamp = entry.timestamp();
        items.push_back(item);
    }
    return true;
}

bool ServerClient::confirmProcessedItems(const std::vector<Item>& items,
                                         const std::string& date,
                                         std::string& error) const {
    receiptreaderproto::ServerRequest request;
    auto* confirmRequest = request.mutable_confirm_processed_items();
    confirmRequest->set_date(date);
    for (const auto& item : items) {
        auto* entry = confirmRequest->add_items();
        entry->set_code(item.code);
        entry->set_description(item.description);
        entry->set_price(item.price);
        entry->set_timestamp(item.timestamp);
    }

    if (!sendRequest(request, error)) {
        return false;
    }

    receiptreaderproto::ServerResponse response;
    if (!receiveResponse(response, error)) {
        return false;
    }

    if (!response.has_status()) {
        error = "Unexpected server response.";
        return false;
    }
    if (!response.status().success()) {
        error = response.status().message();
        return false;
    }
    return true;
}

bool ServerClient::processImages(const std::string& receiptDir,
                                 const std::function<void(int, int, const std::string&)>& onProgress,
                                 std::string& error) const {
    receiptreaderproto::ServerRequest request;
    auto* process = request.mutable_process_images();
    process->set_receipt_dir(receiptDir);

    if (!sendRequest(request, error)) {
        return false;
    }

    while (true) {
        receiptreaderproto::ServerResponse response;
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
