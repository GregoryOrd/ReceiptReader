#include <gtest/gtest.h>

#include "server/guiComms/WebServer.h"
#include "server/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;

class ServerIntegrationTest : public ::testing::Test {
protected:
    static constexpr int HTTP_PORT = 53001;
    static inline std::unique_ptr<Server> server;
    static inline std::unique_ptr<WebServer> webServer;
    static inline std::thread webThread;
    static inline std::string dbPath;

    static std::string makeTempDbPath() {
        const char* tmp = std::getenv("TMPDIR");
        if (!tmp) tmp = "/tmp";
        return std::string(tmp) + "/receipt_integration_test_" + std::to_string(std::time(nullptr)) + ".db";
    }

    static void SetUpTestSuite() {
        dbPath = makeTempDbPath();
        fs::remove(dbPath);
        server = std::make_unique<Server>(dbPath);
        if (!server->initialize()) {
            throw std::runtime_error("Failed to initialize server database");
        }
        webServer = std::make_unique<WebServer>(HTTP_PORT, *server);
        webThread = std::thread([]() { webServer->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    static void TearDownTestSuite() {
        if (webThread.joinable()) {
            webThread.detach();
        }
        if (!dbPath.empty()) {
            fs::remove(dbPath);
        }
    }

    int connectWithRetry() const {
        for (int attempt = 0; attempt < 20; ++attempt) {
            const int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return -1;
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(HTTP_PORT);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                return sock;
            }
            close(sock);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return -1;
    }

    std::string sendHttpRequest(const std::string& method,
                               const std::string& path,
                               const std::string& body = "",
                               const std::string& contentType = "application/json") {
        const int sock = connectWithRetry();
        if (sock < 0) return "";
        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: 127.0.0.1:" << HTTP_PORT << "\r\n";
        request << "Connection: close\r\n";
        if (!body.empty()) {
            request << "Content-Type: " << contentType << "\r\n";
            request << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n" << body;
        const std::string data = request.str();
        size_t sent = 0;
        while (sent < data.size()) {
            const ssize_t count = send(sock, data.data() + sent, data.size() - sent, 0);
            if (count <= 0) {
                close(sock);
                return "";
            }
            sent += static_cast<size_t>(count);
        }
        std::string response;
        char buffer[4096];
        ssize_t count;
        while ((count = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
            response.append(buffer, static_cast<size_t>(count));
        }
        close(sock);
        return response;
    }

    int parseHttpStatus(const std::string& response) const {
        const size_t firstSpace = response.find(' ');
        if (firstSpace == std::string::npos) return -1;
        const size_t secondSpace = response.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos) return -1;
        return std::stoi(response.substr(firstSpace + 1, secondSpace - firstSpace - 1));
    }

    static std::string multipartBody(const std::string& field, const std::string& filename, const std::string& data) {
        constexpr char boundary[] = "ReceiptReaderTestBoundary";
        std::string body = "--" + std::string(boundary) + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + field + "\"; filename=\"" + filename + "\"\r\n";
        body += "Content-Type: image/jpeg\r\n\r\n";
        body += data;
        body += "\r\n--" + std::string(boundary) + "--\r\n";
        return body;
    }

    static constexpr const char* multipartContentType() {
        return "multipart/form-data; boundary=ReceiptReaderTestBoundary";
    }
};

TEST_F(ServerIntegrationTest, ConfirmItemsHttp) {
    const std::string body = R"({"date":"2025-04-11","items":[{"code":"HTTP_CODE","description":"HTTP Item","price":5.55,"is_unit_price":false}]})";
    const std::string response = sendHttpRequest("POST", "/api/v1/receipts/confirm", body);
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("\"success\":true"), std::string::npos);
}

TEST_F(ServerIntegrationTest, GetItemsHttpReturnsFullSummary) {
    const std::string response = sendHttpRequest("GET", "/api/v1/items?code=HTTP_CODE");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("\"current_price\""), std::string::npos);
    EXPECT_NE(response.find("\"timestamp_first\""), std::string::npos);
    EXPECT_NE(response.find("HTTP_CODE"), std::string::npos);
}

TEST_F(ServerIntegrationTest, GetItemHistoryHttp) {
    const std::string response = sendHttpRequest("GET", "/api/v1/items/HTTP_CODE/history");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("\"timestamp\""), std::string::npos);
    EXPECT_NE(response.find("\"price\""), std::string::npos);
}

TEST_F(ServerIntegrationTest, ProcessImageHttpAcceptsMultipartUpload) {
    const std::string body = multipartBody("file", "test.jpg", "not-a-real-image");
    const std::string response = sendHttpRequest("POST", "/api/v1/receipts/process", body, multipartContentType());
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("\"items\""), std::string::npos);
}

TEST_F(ServerIntegrationTest, StartBatchProcessingHttp) {
    std::string body = multipartBody("files", "one.jpg", "not-a-real-image");
    constexpr char boundary[] = "ReceiptReaderTestBoundary";
    body += "--" + std::string(boundary) + "\r\n";
    body += "Content-Disposition: form-data; name=\"files\"; filename=\"two.jpg\"\r\n";
    body += "Content-Type: image/jpeg\r\n\r\n";
    body += "also-not-a-real-image";
    body += "\r\n--" + std::string(boundary) + "--\r\n";
    const std::string response = sendHttpRequest("POST", "/api/v1/receipts/batch-jobs", body, multipartContentType());
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 202);
    EXPECT_NE(response.find("\"job_id\""), std::string::npos);
    EXPECT_NE(response.find("\"total_images\":2"), std::string::npos);

    std::smatch match;
    ASSERT_TRUE(std::regex_search(response, match, std::regex("\"job_id\":\"([^\"]+)\"")));
    const std::string jobPath = "/api/v1/receipts/batch-jobs/" + match[1].str();
    std::string jobResponse;
    for (int attempt = 0; attempt < 100; ++attempt) {
        jobResponse = sendHttpRequest("GET", jobPath);
        if (jobResponse.find("\"status\":\"completed\"") != std::string::npos ||
            jobResponse.find("\"status\":\"failed\"") != std::string::npos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(parseHttpStatus(jobResponse), 200);
    EXPECT_NE(jobResponse.find("\"status\":\"completed\""), std::string::npos);
}

TEST_F(ServerIntegrationTest, HealthAndNotFoundHttp) {
    const std::string health = sendHttpRequest("GET", "/api/v1/health");
    ASSERT_FALSE(health.empty());
    EXPECT_EQ(parseHttpStatus(health), 200);
    EXPECT_NE(health.find("\"status\":\"ok\""), std::string::npos);

    const std::string missing = sendHttpRequest("GET", "/api/v1/unknown");
    ASSERT_FALSE(missing.empty());
    EXPECT_EQ(parseHttpStatus(missing), 404);
}

TEST_F(ServerIntegrationTest, ServesWebFrontend) {
    const std::string response = sendHttpRequest("GET", "/");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 200);
    EXPECT_NE(response.find("text/html"), std::string::npos);
    EXPECT_NE(response.find("<div id=\"app\"></div>"), std::string::npos);
}

TEST_F(ServerIntegrationTest, InvalidImageUploadReturnsBadRequest) {
    const std::string response = sendHttpRequest("POST", "/api/v1/receipts/process", R"({"filename":"test.jpg"})");
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(parseHttpStatus(response), 400);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
