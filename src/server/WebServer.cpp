#include "WebServer.h"
#include "server.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <strings.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    for (char c : value) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    escaped += buf;
                } else {
                    escaped += c;
                }
        }
    }
    return escaped;
}

std::string decodeUrlEncoded(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        char c = encoded[i];
        if (c == '%') {
            if (i + 2 < encoded.size()) {
                int hi = std::isdigit(encoded[i + 1]) ? encoded[i + 1] - '0' : std::toupper(encoded[i + 1]) - 'A' + 10;
                int lo = std::isdigit(encoded[i + 2]) ? encoded[i + 2] - '0' : std::toupper(encoded[i + 2]) - 'A' + 10;
                decoded += static_cast<char>((hi << 4) | lo);
                i += 2;
            }
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }
    return decoded;
}

bool parseQueryString(const std::string& query, std::vector<std::pair<std::string, std::string>>& params) {
    size_t pos = 0;
    while (pos < query.size()) {
        size_t eq = query.find('=', pos);
        if (eq == std::string::npos) {
            break;
        }
        size_t amp = query.find('&', eq + 1);
        std::string key = query.substr(pos, eq - pos);
        std::string value = query.substr(eq + 1, amp == std::string::npos ? std::string::npos : amp - eq - 1);
        params.emplace_back(decodeUrlEncoded(key), decodeUrlEncoded(value));
        if (amp == std::string::npos) {
            break;
        }
        pos = amp + 1;
    }
    return true;
}

bool parseJsonStringField(const std::string& json, const std::string& field, std::string& output) {
    std::string needle = '"' + field + '"';
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) {
        return false;
    }
    size_t end = pos + 1;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\' && end + 1 < json.size()) {
            end += 2;
        } else {
            ++end;
        }
    }
    if (end >= json.size()) {
        return false;
    }
    output.clear();
    for (size_t i = pos + 1; i < end; ++i) {
        if (json[i] == '\\' && i + 1 < end) {
            switch (json[i + 1]) {
                case '"': output += '"'; break;
                case '\\': output += '\\'; break;
                case '/': output += '/'; break;
                case 'b': output += '\b'; break;
                case 'f': output += '\f'; break;
                case 'n': output += '\n'; break;
                case 'r': output += '\r'; break;
                case 't': output += '\t'; break;
                default: output += json[i + 1]; break;
            }
            ++i;
        } else {
            output += json[i];
        }
    }
    return true;
}

bool parseJsonDoubleField(const std::string& json, const std::string& field, double& output) {
    std::string needle = '"' + field + '"';
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return false;
    }
    pos++;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }
    size_t end = pos;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '.' || json[end] == '-' || json[end] == '+' || json[end] == 'e' || json[end] == 'E')) {
        end++;
    }
    if (end == pos) {
        return false;
    }
    try {
        output = std::stod(json.substr(pos, end - pos));
    } catch (...) {
        return false;
    }
    return true;
}

bool parseJsonItems(const std::string& json, std::vector<Item>& items) {
    size_t itemsKey = json.find("\"items\"");
    if (itemsKey == std::string::npos) {
        return false;
    }
    size_t arrayStart = json.find('[', itemsKey);
    size_t arrayEnd = json.find(']', arrayStart);
    if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
        return false;
    }
    size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        pos = json.find('{', pos);
        if (pos == std::string::npos || pos >= arrayEnd) {
            break;
        }
        size_t objEnd = json.find('}', pos);
        if (objEnd == std::string::npos || objEnd >= arrayEnd) {
            return false;
        }
        std::string object = json.substr(pos, objEnd - pos + 1);
        Item item;
        if (!parseJsonStringField(object, "code", item.code) ||
            !parseJsonStringField(object, "description", item.description) ||
            !parseJsonStringField(object, "timestamp", item.timestamp) ||
            !parseJsonDoubleField(object, "price", item.price)) {
            return false;
        }
        items.push_back(item);
        pos = objEnd + 1;
    }
    return !items.empty();
}

bool base64Decode(const std::string& input, std::string& output) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> decodeTable(256, -1);
    for (int i = 0; i < static_cast<int>(alphabet.size()); ++i) {
        decodeTable[static_cast<unsigned char>(alphabet[i])] = i;
    }

    output.clear();
    int val = 0;
    int bits = -8;
    for (char c : input) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '=') {
            continue;
        }
        int d = decodeTable[static_cast<unsigned char>(c)];
        if (d == -1) {
            return false;
        }
        val = (val << 6) + d;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return true;
}

} // namespace

WebServer::WebServer(int port, Server& server)
    : m_port(port), m_server(server) {
}

bool WebServer::run() {
    int serverSock = createServerSocket();
    if (serverSock < 0) {
        return false;
    }

    if (!bindAndListen(serverSock)) {
        close(serverSock);
        return false;
    }

    std::cout << "Web server listening on port " << m_port << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSock < 0) {
            std::cerr << "Failed to accept HTTP connection: " << strerror(errno) << std::endl;
            continue;
        }

        std::thread worker([this, clientSock]() {
            handleConnection(clientSock);
            close(clientSock);
        });
        worker.detach();
    }

    close(serverSock);
    return true;
}

int WebServer::createServerSocket() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "Failed to create HTTP server socket: " << strerror(errno) << std::endl;
        return -1;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return serverSock;
}

bool WebServer::bindAndListen(int serverSock) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

    if (bind(serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind HTTP server socket: " << strerror(errno) << std::endl;
        return false;
    }

    if (listen(serverSock, 10) < 0) {
        std::cerr << "HTTP server failed to listen: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool WebServer::handleConnection(int clientSock) {
    std::string method;
    std::string pathAndQuery;
    std::string query;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    if (!receiveHttpRequest(clientSock, method, pathAndQuery, query, body, headers)) {
        return false;
    }

    std::string path = extractPath(pathAndQuery, query);
    std::vector<std::pair<std::string, std::string>> queryParams;
    if (!query.empty()) {
        parseQueryString(query, queryParams);
    }

    if (method == "GET" && path == "/items") {
        std::string code;
        std::string priceMin;
        std::string priceMax;
        std::string dateStart;
        std::string dateEnd;
        bool orderByTimestamp = false;
        for (const auto& [key, value] : queryParams) {
            if (key == "code") {
                code = value;
            } else if (key == "price_min") {
                priceMin = value;
            } else if (key == "price_max") {
                priceMax = value;
            } else if (key == "date_start") {
                dateStart = value;
            } else if (key == "date_end") {
                dateEnd = value;
            } else if (key == "order_by_timestamp") {
                orderByTimestamp = (value == "true" || value == "1");
            }
        }

        auto items = m_server.queryItemsFiltered(code, priceMin, priceMax, dateStart, dateEnd, orderByTimestamp);
        std::ostringstream json;
        json << "{\"items\": [";
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            if (i > 0) json << ",";
            json << "{\"code\":\"" << escapeJsonString(item.code) << "\",";
            json << "\"description\":\"" << escapeJsonString(item.description) << "\",";
            json << "\"price\":" << item.price << ",";
            json << "\"timestamp\":\"" << escapeJsonString(item.timestamp) << "\"}";
        }
        json << "]}";
        sendJson(clientSock, 200, json.str());
        return true;
    }

    if (method == "POST" && path == "/process_images") {
        std::string receiptDir;
        if (!parseJsonStringField(body, "receipt_dir", receiptDir)) {
            sendJson(clientSock, 400, "{\"error\":\"Missing receipt_dir\"}");
            return true;
        }

        receiptreader::ProcessComplete complete;
        bool success = m_server.processImagesDirectory(receiptDir, [&](const receiptreader::ProcessProgress&) {
            return true;
        }, complete);

        std::ostringstream json;
        json << "{\"success\":" << (success ? "true" : "false")
             << ",\"total_items\":" << complete.total_items()
             << ",\"message\":\"" << escapeJsonString(complete.message()) << "\"}";
        sendJson(clientSock, success ? 200 : 500, json.str());
        return true;
    }

    if (method == "POST" && path == "/process_image") {
        std::string filename;
        std::string imageDataBase64;
        if (!parseJsonStringField(body, "filename", filename) || !parseJsonStringField(body, "image_data", imageDataBase64)) {
            sendJson(clientSock, 400, "{\"error\":\"Missing filename or image_data\"}");
            return true;
        }

        std::string imageData;
        if (!base64Decode(imageDataBase64, imageData)) {
            sendJson(clientSock, 400, "{\"error\":\"Invalid base64 image_data\"}");
            return true;
        }

        auto items = m_server.processImageBytes(filename, imageData);
        std::ostringstream json;
        json << "{\"items\": [";
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            if (i > 0) json << ",";
            json << "{\"code\":\"" << escapeJsonString(item.code) << "\",";
            json << "\"description\":\"" << escapeJsonString(item.description) << "\",";
            json << "\"price\":" << item.price << ",";
            json << "\"timestamp\":\"" << escapeJsonString(item.timestamp) << "\"}";
        }
        json << "]}";
        sendJson(clientSock, 200, json.str());
        return true;
    }

    if (method == "POST" && path == "/confirm_processed_items") {
        std::vector<Item> items;
        if (!parseJsonItems(body, items)) {
            sendJson(clientSock, 400, "{\"error\":\"Invalid items payload\"}");
            return true;
        }

        std::string date;
        if (!parseJsonStringField(body, "date", date)) {
            sendJson(clientSock, 400, "{\"error\":\"Missing date field\"}");
            return true;
        }

        for (auto& item : items) {
            item.timestamp = date;
        }

        bool success = m_server.confirmProcessedItems(items);
        sendJson(clientSock, success ? 200 : 500, success ? "{\"success\":true}" : "{\"success\":false}");
        return true;
    }

    sendJson(clientSock, 404, "{\"error\":\"Not found\"}");
    return true;
}

bool WebServer::receiveHttpRequest(int clientSock,
                                   std::string& method,
                                   std::string& path,
                                   std::string& query,
                                   std::string& body,
                                   std::vector<std::pair<std::string, std::string>>& headers) {
    std::string request;
    char buffer[4096];
    ssize_t received;
    while ((received = recv(clientSock, buffer, sizeof(buffer), 0)) > 0) {
        request.append(buffer, received);
        if (request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }
    if (received <= 0 && request.empty()) {
        return false;
    }

    size_t headerEnd = request.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }

    std::istringstream stream(request.substr(0, headerEnd));
    std::string requestLine;
    if (!std::getline(stream, requestLine) || requestLine.empty()) {
        return false;
    }

    size_t methodEnd = requestLine.find(' ');
    size_t pathEnd = requestLine.find(' ', methodEnd + 1);
    if (methodEnd == std::string::npos || pathEnd == std::string::npos) {
        return false;
    }

    method = requestLine.substr(0, methodEnd);
    path = requestLine.substr(methodEnd + 1, pathEnd - methodEnd - 1);
    query.clear();
    size_t queryStart = path.find('?');
    if (queryStart != std::string::npos) {
        query = path.substr(queryStart + 1);
        path = path.substr(0, queryStart);
    }

    std::string headerLine;
    size_t contentLength = 0;
    while (std::getline(stream, headerLine) && !headerLine.empty()) {
        if (headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        size_t colon = headerLine.find(':');
        if (colon != std::string::npos) {
            std::string name = headerLine.substr(0, colon);
            std::string value = headerLine.substr(colon + 1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                value.erase(value.begin());
            }
            headers.emplace_back(name, value);
            if (strcasecmp(name.c_str(), "Content-Length") == 0) {
                contentLength = std::stoul(value);
            }
        }
    }

    size_t bodyStart = headerEnd + 4;
    if (request.size() > bodyStart) {
        body = request.substr(bodyStart);
    }
    while (body.size() < contentLength) {
        received = recv(clientSock, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        body.append(buffer, received);
    }
    return true;
}

void WebServer::sendResponse(int clientSock, int statusCode, const std::string& statusText, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;
    std::string data = response.str();
    send(clientSock, data.data(), data.size(), 0);
}

void WebServer::sendJson(int clientSock, int statusCode, const std::string& body) {
    const char* statusText = statusCode == 200 ? "OK" : (statusCode == 400 ? "Bad Request" : (statusCode == 404 ? "Not Found" : "Internal Server Error"));
    sendResponse(clientSock, statusCode, statusText, body);
}

std::string WebServer::extractPath(const std::string& pathAndQuery, std::string& query) {
    size_t queryStart = pathAndQuery.find('?');
    if (queryStart == std::string::npos) {
        query.clear();
        return pathAndQuery;
    }
    query = pathAndQuery.substr(queryStart + 1);
    return pathAndQuery.substr(0, queryStart);
}
