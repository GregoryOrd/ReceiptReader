#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <filesystem>
#include "server/ProtobufServer.h"
#include "server/WebServer.h"
#include "server/server.h"
#include "ipc/message_io.h"
#include "processor.pb.h"

namespace fs = std::filesystem;

class ServerIntegrationTest : public ::testing::Test {
protected:
    static constexpr int PROTO_PORT = 52001;
    static constexpr int HTTP_PORT = 53001;
    static inline std::unique_ptr<Server> server;
    static inline std::unique_ptr<ProtobufServer> protobufServer;
    static inline std::unique_ptr<WebServer> webServer;
    static inline std::thread protoThread;
    static inline std::thread webThread;
    static inline std::string dbPath;
    static inline std::vector<std::string> tempDirs;

    static std::string makeTempDbPath() {
        const char* tmp = std::getenv("TMPDIR");
        if (!tmp) tmp = "/tmp";
        return std::string(tmp) + "/receipt_integration_test_" + std::to_string(std::time(nullptr)) + ".db";
    }

    static std::string makeTempReceiptDirPath() {
        const char* tmp = std::getenv("TMPDIR");
        if (!tmp) tmp = "/tmp";
        std::string path = std::string(tmp) + "/receipt_integration_dir_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        fs::create_directories(path);
        tempDirs.push_back(path);
        return path;
    }

    static bool createDummyReceiptFile(const std::string& dir) {
        fs::path directory(dir);
        if (!fs::exists(directory) && !fs::create_directories(directory)) {
            return false;
        }
        std::ofstream out(directory / "receipt.jpg", std::ios::binary);
        if (!out) {
            return false;
        }
        out << "dummy image data";
        return true;
    }

    static void SetUpTestSuite() {
        dbPath = makeTempDbPath();
        fs::remove(dbPath);
        server = std::make_unique<Server>(dbPath);
        if (!server->initialize()) {
            throw std::runtime_error("Failed to initialize server database");
        }

        protobufServer = std::make_unique<ProtobufServer>(PROTO_PORT, *server);
        webServer = std::make_unique<WebServer>(HTTP_PORT, *server);

        protoThread = std::thread([]() { protobufServer->run(); });
        webThread = std::thread([]() { webServer->run(); });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    static void TearDownTestSuite() {
        if (protoThread.joinable()) {
            protoThread.detach();
        }
        if (webThread.joinable()) {
            webThread.detach();
        }
        if (!dbPath.empty()) {
            fs::remove(dbPath);
        }
        for (const auto& dir : tempDirs) {
            fs::remove_all(dir);
        }
    }

    int connectWithRetry(int port) const {
        int attempts = 10;
        while (attempts--) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return -1;
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                return sock;
            }
            close(sock);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return -1;
    }

    bool sendProtobufRequest(int sock, const receiptreader::ServerRequest& request, receiptreader::ServerResponse& response) {
        return ipc::sendProtobufMessage(sock, request) && ipc::receiveProtobufMessage(sock, &response);
    }

    std::string sendHttpRequest(int sock, const std::string& method, const std::string& path, const std::string& body = "") {
        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: 127.0.0.1:" << HTTP_PORT << "\r\n";
        request << "Connection: close\r\n";
        if (!body.empty()) {
            request << "Content-Type: application/json\r\n";
            request << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n";
        request << body;
        std::string req = request.str();
        if (send(sock, req.data(), req.size(), 0) < 0) return "";
        std::string resp;
        char buffer[4096];
        ssize_t n;
        while ((n = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
            resp.append(buffer, n);
        }
        return resp;
    }

    int parseHttpStatus(const std::string& response) const {
        size_t pos1 = response.find(' ');
        if (pos1 == std::string::npos) return -1;
        size_t pos2 = response.find(' ', pos1 + 1);
        if (pos2 == std::string::npos) return -1;
        return std::stoi(response.substr(pos1 + 1, pos2 - pos1 - 1));
    }
};

TEST_F(ServerIntegrationTest, ConfirmProcessedItemsProtobuf) {
    int sock = connectWithRetry(PROTO_PORT);
    ASSERT_GE(sock, 0);
    receiptreader::ServerRequest request;
    auto* confirm = request.mutable_confirm_processed_items();
    auto* item = confirm->add_items();
    item->set_code("PROTO_CODE");
    item->set_description("Proto Item");
    item->set_price(9.99);
    item->set_timestamp("2025-04-11");
    receiptreader::ServerResponse response;
    ASSERT_TRUE(sendProtobufRequest(sock, request, response));
    ASSERT_TRUE(response.has_status());
    close(sock);
}

TEST_F(ServerIntegrationTest, QueryItemsProtobuf) {
    int sock = connectWithRetry(PROTO_PORT);
    ASSERT_GE(sock, 0);
    receiptreader::ServerRequest request;
    auto* query = request.mutable_query_items();
    query->set_code("PROTO_CODE");
    query->set_price_min("");
    query->set_price_max("");
    query->set_date_start("");
    query->set_date_end("");
    query->set_order_by_timestamp(false);
    receiptreader::ServerResponse response;
    ASSERT_TRUE(sendProtobufRequest(sock, request, response));
    ASSERT_TRUE(response.has_query_items_response());
    close(sock);
}

TEST_F(ServerIntegrationTest, ProcessImageProtobuf) {
    int sock = connectWithRetry(PROTO_PORT);
    ASSERT_GE(sock, 0);
    receiptreader::ServerRequest request;
    auto* proc = request.mutable_process_image();
    proc->set_filename("test.jpg");
    proc->set_image_data("dGVzdA==");
    receiptreader::ServerResponse response;
    ASSERT_TRUE(sendProtobufRequest(sock, request, response));
    ASSERT_TRUE(response.has_process_image_response() || response.has_status());
    close(sock);
}

TEST_F(ServerIntegrationTest, ProcessImagesProtobuf) {
    std::string receiptDir = makeTempReceiptDirPath();
    ASSERT_TRUE(createDummyReceiptFile(receiptDir));

    int sock = connectWithRetry(PROTO_PORT);
    ASSERT_GE(sock, 0);
    receiptreader::ServerRequest request;
    auto* proc = request.mutable_process_images();
    proc->set_receipt_dir(receiptDir);
    receiptreader::ServerResponse response;
    ASSERT_TRUE(sendProtobufRequest(sock, request, response));
    ASSERT_TRUE(response.has_complete() || response.has_progress() || response.has_status());
    close(sock);
}

TEST_F(ServerIntegrationTest, ConfirmProcessedItemsHttp) {
    int sock = connectWithRetry(HTTP_PORT);
    ASSERT_GE(sock, 0);
    std::string body = R"({"date":"2025-04-11","items":[{"code":"HTTP_CODE","description":"HTTP Item","price":5.55,"timestamp":"2025-04-11"}]})";
    std::string response = sendHttpRequest(sock, "POST", "/confirm_processed_items", body);
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    close(sock);
}

TEST_F(ServerIntegrationTest, GetItemsHttp) {
    int sock = connectWithRetry(HTTP_PORT);
    ASSERT_GE(sock, 0);
    std::string response = sendHttpRequest(sock, "GET", "/items?code=HTTP_CODE");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("HTTP_CODE"), std::string::npos);
    close(sock);
}

TEST_F(ServerIntegrationTest, ProcessImageHttp) {
    int sock = connectWithRetry(HTTP_PORT);
    ASSERT_GE(sock, 0);
    std::string body = R"({"filename":"test.jpg","image_data":"dGVzdA=="})";
    std::string response = sendHttpRequest(sock, "POST", "/process_image", body);
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    close(sock);
}

TEST_F(ServerIntegrationTest, ProcessImagesHttp) {
    std::string receiptDir = makeTempReceiptDirPath();
    ASSERT_TRUE(createDummyReceiptFile(receiptDir));

    int sock = connectWithRetry(HTTP_PORT);
    ASSERT_GE(sock, 0);
    std::string body = std::string("{\"receipt_dir\":\"") + receiptDir + "\"}";
    std::string response = sendHttpRequest(sock, "POST", "/process_images", body);
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    close(sock);
}

TEST_F(ServerIntegrationTest, HttpNotFound) {
    int sock = connectWithRetry(HTTP_PORT);
    ASSERT_GE(sock, 0);
    std::string response = sendHttpRequest(sock, "GET", "/unknown_endpoint");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 404);
    close(sock);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
