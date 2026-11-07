#include "WebServer.h"

#include "common/Item.h"
#include "common/PriceHistorySummary.h"
#include "server.h"

#include <QByteArray>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUuid>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr size_t MAX_REQUEST_SIZE = 100 * 1024 * 1024;
constexpr size_t MAX_HEADER_SIZE = 64 * 1024;
constexpr char API_PREFIX[] = "/api/v1";

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

struct UploadedImage {
    std::string filename;
    std::string data;
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return first < last ? std::string(first, last) : std::string();
}

bool sendAll(int socket, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t count = send(socket, data.data() + sent, data.size() - sent, 0);
        if (count <= 0) {
            return false;
        }
        sent += static_cast<size_t>(count);
    }
    return true;
}

std::string statusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}

std::string contentTypeForPath(const std::filesystem::path& path) {
    const std::string extension = lower(path.extension().string());
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

QJsonObject problem(int status, const std::string& title, const std::string& detail = {}) {
    QJsonObject result;
    result.insert("title", QString::fromStdString(title));
    result.insert("status", status);
    if (!detail.empty()) {
        result.insert("detail", QString::fromStdString(detail));
    }
    return result;
}

QJsonObject itemToJson(const Item& item) {
    QJsonObject json;
    json.insert("code", QString::fromStdString(item.code));
    json.insert("description", QString::fromStdString(item.description));
    json.insert("price", item.price);
    json.insert("timestamp", QString::fromStdString(item.timestamp));
    json.insert("is_unit_price", item.isUnitPrice);
    return json;
}

QJsonObject summaryToJson(const PriceHistorySummary& item) {
    QJsonObject json;
    json.insert("code", QString::fromStdString(item.code()));
    json.insert("description", QString::fromStdString(item.description()));
    json.insert("min_price", item.minPrice());
    json.insert("max_price", item.maxPrice());
    json.insert("current_price", item.currentPrice());
    json.insert("is_unit_price", item.isUnitPrice());
    json.insert("timestamp_first", QString::fromStdString(item.timestampFirst()));
    json.insert("timestamp_last", QString::fromStdString(item.timestampLast()));
    json.insert("lifetime_inflation_rate", item.liftimeInflationRate());
    json.insert("one_year_inflation_rate", item.oneYearInflationRate());
    json.insert("six_month_inflation_rate", item.sixMonthInflationRate());
    json.insert("three_month_inflation_rate", item.threeMonthInflationRate());
    json.insert("one_month_inflation_rate", item.oneMonthInflationRate());
    json.insert("latest_price_change", item.latestPriceChange());
    json.insert("times_purchased", item.timesPurchased());
    return json;
}

bool receiveRequest(int socket, HttpRequest& request, int& errorStatus, std::string& error) {
    std::string raw;
    char buffer[8192];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        const ssize_t count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            return false;
        }
        raw.append(buffer, static_cast<size_t>(count));
        if (raw.size() > MAX_HEADER_SIZE) {
            errorStatus = 400;
            error = "HTTP headers exceed the allowed size.";
            return false;
        }
        headerEnd = raw.find("\r\n\r\n");
    }

    std::istringstream headers(raw.substr(0, headerEnd));
    std::string requestLine;
    if (!std::getline(headers, requestLine)) {
        errorStatus = 400;
        error = "Invalid HTTP request line.";
        return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }
    std::istringstream requestLineStream(requestLine);
    std::string target;
    std::string version;
    if (!(requestLineStream >> request.method >> target >> version) || version.rfind("HTTP/", 0) != 0) {
        errorStatus = 400;
        error = "Invalid HTTP request line.";
        return false;
    }

    const size_t queryStart = target.find('?');
    request.path = target.substr(0, queryStart);
    if (queryStart != std::string::npos) {
        request.query = target.substr(queryStart + 1);
    }

    size_t contentLength = 0;
    std::string line;
    while (std::getline(headers, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string name = lower(trim(line.substr(0, colon)));
        const std::string value = trim(line.substr(colon + 1));
        request.headers[name] = value;
        if (name == "content-length") {
            try {
                contentLength = std::stoull(value);
            } catch (const std::exception&) {
                errorStatus = 400;
                error = "Invalid Content-Length header.";
                return false;
            }
        }
        if (name == "transfer-encoding" && lower(value).find("chunked") != std::string::npos) {
            errorStatus = 400;
            error = "Chunked transfer encoding is not supported.";
            return false;
        }
    }

    if (contentLength > MAX_REQUEST_SIZE) {
        errorStatus = 413;
        error = "Request exceeds the 100 MiB upload limit.";
        return false;
    }

    const size_t bodyStart = headerEnd + 4;
    if (raw.size() > bodyStart) {
        request.body = raw.substr(bodyStart);
    }
    if (request.body.size() > contentLength) {
        request.body.resize(contentLength);
    }
    while (request.body.size() < contentLength) {
        const size_t remaining = contentLength - request.body.size();
        const ssize_t count = recv(socket, buffer, std::min(sizeof(buffer), remaining), 0);
        if (count <= 0) {
            errorStatus = 400;
            error = "Request body ended before Content-Length bytes were received.";
            return false;
        }
        request.body.append(buffer, static_cast<size_t>(count));
    }
    return true;
}

std::string decodeComponent(const std::string& value) {
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '+', ' ');
    const QByteArray decoded = QByteArray::fromPercentEncoding(QByteArray::fromStdString(normalized));
    return decoded.toStdString();
}

std::unordered_map<std::string, std::string> parseQuery(const std::string& query) {
    std::unordered_map<std::string, std::string> values;
    size_t start = 0;
    while (start <= query.size()) {
        const size_t end = query.find('&', start);
        const std::string pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const size_t equals = pair.find('=');
        const std::string key = decodeComponent(pair.substr(0, equals));
        const std::string value = equals == std::string::npos ? "" : decodeComponent(pair.substr(equals + 1));
        if (!key.empty()) {
            values[key] = value;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return values;
}

bool parseMultipart(const HttpRequest& request,
                    const std::string& expectedField,
                    std::vector<UploadedImage>& files,
                    std::string& error) {
    const auto contentType = request.headers.find("content-type");
    if (contentType == request.headers.end() || lower(contentType->second).find("multipart/form-data") != 0) {
        error = "Expected multipart/form-data.";
        return false;
    }
    const size_t boundaryKey = lower(contentType->second).find("boundary=");
    if (boundaryKey == std::string::npos) {
        error = "Multipart boundary is missing.";
        return false;
    }
    std::string boundary = trim(contentType->second.substr(boundaryKey + 9));
    const size_t parameterEnd = boundary.find(';');
    if (parameterEnd != std::string::npos) {
        boundary = trim(boundary.substr(0, parameterEnd));
    }
    if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    if (boundary.empty() || boundary.find(';') != std::string::npos) {
        error = "Invalid multipart boundary.";
        return false;
    }

    const std::string delimiter = "--" + boundary;
    size_t position = request.body.find(delimiter);
    if (position == std::string::npos) {
        error = "Multipart boundary was not found in the request body.";
        return false;
    }
    while (position != std::string::npos) {
        position += delimiter.size();
        if (request.body.compare(position, 2, "--") == 0) {
            break;
        }
        if (request.body.compare(position, 2, "\r\n") != 0) {
            error = "Malformed multipart boundary.";
            return false;
        }
        position += 2;
        const size_t headerEnd = request.body.find("\r\n\r\n", position);
        if (headerEnd == std::string::npos) {
            error = "Malformed multipart headers.";
            return false;
        }
        const std::string partHeaders = request.body.substr(position, headerEnd - position);
        const std::string normalizedHeaders = lower(partHeaders);
        const size_t dataStart = headerEnd + 4;
        const std::string nextDelimiter = "\r\n" + delimiter;
        const size_t dataEnd = request.body.find(nextDelimiter, dataStart);
        if (dataEnd == std::string::npos) {
            error = "Multipart part is missing its terminating boundary.";
            return false;
        }
        const size_t dispositionStart = normalizedHeaders.find("content-disposition:");
        if (dispositionStart != std::string::npos && normalizedHeaders.find("filename=\"", dispositionStart) != std::string::npos) {
            const size_t dispositionEnd = partHeaders.find("\r\n", dispositionStart);
            const std::string disposition = partHeaders.substr(dispositionStart,
                dispositionEnd == std::string::npos ? std::string::npos : dispositionEnd - dispositionStart);
            auto quotedParameter = [&disposition](const std::string& key, std::string& value) {
                size_t position = disposition.find(';');
                while (position != std::string::npos) {
                    ++position;
                    while (position < disposition.size() &&
                           std::isspace(static_cast<unsigned char>(disposition[position]))) {
                        ++position;
                    }
                    const size_t equals = disposition.find('=', position);
                    if (equals == std::string::npos) {
                        return false;
                    }
                    const std::string parameterName = lower(trim(disposition.substr(position, equals - position)));
                    size_t valueStart = equals + 1;
                    while (valueStart < disposition.size() &&
                           std::isspace(static_cast<unsigned char>(disposition[valueStart]))) {
                        ++valueStart;
                    }
                    const size_t nextParameter = disposition.find(';', valueStart);
                    if (parameterName == key) {
                        if (valueStart >= disposition.size() || disposition[valueStart] != '"') {
                            return false;
                        }
                        const size_t valueEnd = disposition.find('"', valueStart + 1);
                        if (valueEnd == std::string::npos) {
                            return false;
                        }
                        value = disposition.substr(valueStart + 1, valueEnd - valueStart - 1);
                        return true;
                    }
                    position = nextParameter;
                }
                return false;
            };
            std::string fieldName;
            std::string rawFilename;
            if (!quotedParameter("name", fieldName) || !quotedParameter("filename", rawFilename)) {
                error = "Malformed multipart file metadata.";
                return false;
            }
            if (fieldName != expectedField) {
                error = "Unexpected file field in multipart upload.";
                return false;
            }
            {
                std::string filename = std::move(rawFilename);
                const size_t basenameAt = filename.find_last_of("/\\");
                if (basenameAt != std::string::npos) {
                    filename = filename.substr(basenameAt + 1);
                }
                if (filename.empty()) {
                    error = "Uploaded file must have a filename.";
                    return false;
                }
                const std::string extension = lower(std::filesystem::path(filename).extension().string());
                if (extension != ".jpg" && extension != ".jpeg" && extension != ".png" && extension != ".heic") {
                    error = "Unsupported image type; use JPEG, PNG, or HEIC.";
                    return false;
                }
                if (dataEnd == dataStart) {
                    error = "Uploaded image is empty.";
                    return false;
                }
                files.push_back({std::move(filename), request.body.substr(dataStart, dataEnd - dataStart)});
            }
        }
        position = dataEnd + 2;
    }
    if (files.empty()) {
        error = "No receipt image file was uploaded.";
        return false;
    }
    return true;
}

bool parseJsonObject(const std::string& body, QJsonObject& object, std::string& error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(body), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "Request body must be a valid JSON object.";
        return false;
    }
    object = document.object();
    return true;
}

bool readItem(const QJsonValue& value, Item& item, std::string& error) {
    if (!value.isObject()) {
        error = "Each item must be a JSON object.";
        return false;
    }
    const QJsonObject json = value.toObject();
    if (!json.value("code").isString() || !json.value("description").isString() ||
        !json.value("price").isDouble()) {
        error = "Each item requires string code and description and numeric price fields.";
        return false;
    }
    item.code = json.value("code").toString().toStdString();
    item.description = json.value("description").toString().toStdString();
    item.price = json.value("price").toDouble();
    item.isUnitPrice = json.value("is_unit_price").toBool(false);
    return true;
}

bool validIsoDate(const std::string& value) {
    const QDate date = QDate::fromString(QString::fromStdString(value), Qt::ISODate);
    return date.isValid() && date.toString(Qt::ISODate).toStdString() == value;
}

bool validOptionalNumber(const std::string& value) {
    if (value.empty() || trim(value) != value) {
        return true;
    }
    try {
        size_t parsed = 0;
        const double number = std::stod(value, &parsed);
        return parsed == value.size() && std::isfinite(number);
    } catch (const std::exception&) {
        return false;
    }
}

QJsonObject batchJobJson(const WebServer::BatchJob& job) {
    QJsonObject json;
    json.insert("job_id", QString::fromStdString(job.id));
    json.insert("status", QString::fromStdString(job.status));
    json.insert("processed_images", job.processedImages);
    json.insert("total_images", job.totalImages);
    json.insert("current_image", QString::fromStdString(job.currentImage));
    json.insert("total_items", job.totalItems);
    if (job.status == "completed" || job.status == "failed") {
        json.insert("success", job.success);
        json.insert("message", QString::fromStdString(job.message));
    }
    return json;
}

} // namespace

WebServer::WebServer(int port, Server& server, std::string webRoot)
    : m_port(port), m_server(server), m_webRoot(std::move(webRoot)) {
}

bool WebServer::run() {
    const int serverSock = createServerSocket();
    if (serverSock < 0) {
        return false;
    }
    if (!bindAndListen(serverSock)) {
        close(serverSock);
        return false;
    }

    std::cout << "REST server listening on port " << m_port << std::endl;
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        const int clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSock < 0) {
            std::cerr << "Failed to accept HTTP connection: " << strerror(errno) << std::endl;
            continue;
        }
        std::thread([this, clientSock]() {
            try {
                handleConnection(clientSock);
            } catch (const std::exception& exception) {
                std::cerr << "HTTP request failed: " << exception.what() << std::endl;
                sendJson(clientSock, 500, problem(500, "Internal server error"));
            }
            close(clientSock);
        }).detach();
    }
}

int WebServer::createServerSocket() {
    const int serverSock = socket(AF_INET, SOCK_STREAM, 0);
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
    HttpRequest request;
    int errorStatus = 400;
    std::string error;
    if (!receiveRequest(clientSock, request, errorStatus, error)) {
        if (!error.empty()) {
            sendJson(clientSock, errorStatus, problem(errorStatus, statusText(errorStatus), error));
        }
        return false;
    }

    if (request.method == "OPTIONS") {
        sendResponse(clientSock, 204, "No Content", "", "text/plain", true);
        return true;
    }

    if (request.path == std::string(API_PREFIX) + "/health" && request.method == "GET") {
        QJsonObject body;
        body.insert("status", "ok");
        sendJson(clientSock, 200, body);
        return true;
    }

    if (request.path == std::string(API_PREFIX) + "/items" && request.method == "GET") {
        const auto params = parseQuery(request.query);
        const auto value = [&params](const char* key) {
            const auto found = params.find(key);
            return found == params.end() ? std::string() : found->second;
        };
        const std::string priceMin = value("price_min");
        const std::string priceMax = value("price_max");
        const std::string dateStart = value("date_start");
        const std::string dateEnd = value("date_end");
        if (!validOptionalNumber(priceMin) || !validOptionalNumber(priceMax) ||
            (!dateStart.empty() && !validIsoDate(dateStart)) ||
            (!dateEnd.empty() && !validIsoDate(dateEnd))) {
            sendJson(clientSock, 400, problem(400, "Invalid query", "Price limits must be numbers and dates must use YYYY-MM-DD."));
            return true;
        }
        auto items = m_server.queryPriceHistorySummaries(value("code"), value("desc"), priceMin,
                                                        priceMax, dateStart, dateEnd);
        QJsonArray rows;
        for (const auto& item : items) {
            rows.append(summaryToJson(item));
        }
        sendJson(clientSock, 200, QJsonObject{{"items", rows}});
        return true;
    }

    const std::string itemPrefix = std::string(API_PREFIX) + "/items/";
    const std::string historySuffix = "/history";
    if (request.method == "GET" && request.path.rfind(itemPrefix, 0) == 0 &&
        request.path.size() > itemPrefix.size() + historySuffix.size() &&
        request.path.compare(request.path.size() - historySuffix.size(), historySuffix.size(), historySuffix) == 0) {
        const std::string encodedCode = request.path.substr(itemPrefix.size(), request.path.size() - itemPrefix.size() - historySuffix.size());
        const std::string code = QByteArray::fromPercentEncoding(QByteArray::fromStdString(encodedCode)).toStdString();
        const auto items = m_server.queryItemCode(code);
        QJsonArray rows;
        for (const auto& item : items) {
            rows.append(itemToJson(item));
        }
        sendJson(clientSock, 200, QJsonObject{{"items", rows}});
        return true;
    }

    if (request.path == std::string(API_PREFIX) + "/receipts/process" && request.method == "POST") {
        std::vector<UploadedImage> files;
        if (!parseMultipart(request, "file", files, error) || files.size() != 1) {
            if (error.empty()) {
                error = "Exactly one image must be uploaded in the file field.";
            }
            sendJson(clientSock, 400, problem(400, "Invalid upload", error));
            return true;
        }
        const auto& file = files.front();
        if (file.data.empty()) {
            sendJson(clientSock, 400, problem(400, "Invalid upload", "Uploaded image is empty."));
            return true;
        }
        const auto items = m_server.processImageBytes(file.filename, file.data);
        QJsonArray rows;
        for (const auto& item : items) {
            rows.append(itemToJson(item));
        }
        sendJson(clientSock, 200, QJsonObject{{"items", rows}});
        return true;
    }

    if (request.path == std::string(API_PREFIX) + "/receipts/confirm" && request.method == "POST") {
        QJsonObject json;
        if (!parseJsonObject(request.body, json, error)) {
            sendJson(clientSock, 400, problem(400, "Invalid request", error));
            return true;
        }
        if (!json.value("date").isString() || json.value("date").toString().isEmpty() ||
            !json.value("items").isArray()) {
            sendJson(clientSock, 400, problem(400, "Invalid request", "Both date and items are required."));
            return true;
        }
        const std::string date = json.value("date").toString().toStdString();
        if (!validIsoDate(date)) {
            sendJson(clientSock, 400, problem(400, "Invalid date", "Receipt date must use YYYY-MM-DD."));
            return true;
        }
        std::vector<Item> items;
        const QJsonArray inputItems = json.value("items").toArray();
        for (const auto& value : inputItems) {
            Item item;
            if (!readItem(value, item, error)) {
                sendJson(clientSock, 400, problem(400, "Invalid item", error));
                return true;
            }
            item.timestamp = date;
            items.push_back(std::move(item));
        }
        if (!m_server.confirmProcessedItems(items)) {
            sendJson(clientSock, 500, problem(500, "Confirmation failed", "The server could not save the receipt items."));
            return true;
        }
        sendJson(clientSock, 200, QJsonObject{{"success", true}, {"message", "Items saved to database."}});
        return true;
    }

    if (request.path == std::string(API_PREFIX) + "/receipts/batch-jobs" && request.method == "POST") {
        std::vector<UploadedImage> files;
        if (!parseMultipart(request, "files", files, error)) {
            sendJson(clientSock, 400, problem(400, "Invalid upload", error));
            return true;
        }

        auto job = std::make_shared<BatchJob>();
        job->id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        job->status = "queued";
        job->totalImages = static_cast<int>(files.size());
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_batchJobs[job->id] = job;
        }
        std::thread([this, job, files = std::move(files)]() {
            {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                job->status = "running";
            }
            try {
                for (const auto& file : files) {
                    {
                        std::lock_guard<std::mutex> lock(m_jobsMutex);
                        job->currentImage = file.filename;
                    }
                    const auto items = m_server.processImageBytes(file.filename, file.data);
                    if (!m_server.confirmProcessedItems(items)) {
                        std::lock_guard<std::mutex> lock(m_jobsMutex);
                        job->status = "failed";
                        job->success = false;
                        job->message = "Failed to save processed items.";
                        return;
                    }
                    std::lock_guard<std::mutex> lock(m_jobsMutex);
                    ++job->processedImages;
                    job->totalItems += static_cast<int>(items.size());
                    job->currentImage = file.filename;
                }
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                job->status = "completed";
                job->success = true;
                job->message = "Processing complete.";
            } catch (const std::exception& exception) {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                job->status = "failed";
                job->success = false;
                job->message = exception.what();
            }
        }).detach();

        QJsonObject acceptedJob;
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            acceptedJob = batchJobJson(*job);
        }
        const std::string jobUrl = std::string(API_PREFIX) + "/receipts/batch-jobs/" + job->id;
        const std::string serialized = QJsonDocument(acceptedJob).toJson(QJsonDocument::Compact).toStdString();
        sendResponse(clientSock, 202, "Accepted", serialized, "application/json; charset=utf-8", false, jobUrl);
        return true;
    }

    const std::string jobsPrefix = std::string(API_PREFIX) + "/receipts/batch-jobs/";
    if (request.method == "GET" && request.path.rfind(jobsPrefix, 0) == 0) {
        const std::string jobId = request.path.substr(jobsPrefix.size());
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        const auto found = m_batchJobs.find(jobId);
        if (found == m_batchJobs.end()) {
            sendJson(clientSock, 404, problem(404, "Job not found"));
            return true;
        }
        sendJson(clientSock, 200, batchJobJson(*found->second));
        return true;
    }

    if (request.path.rfind(API_PREFIX, 0) == 0) {
        sendJson(clientSock, 404, problem(404, "Not found", "The requested API resource does not exist."));
        return true;
    }
    if (request.method == "GET") {
        return serveStatic(clientSock, request.path);
    }
    sendJson(clientSock, 405, problem(405, "Method not allowed"));
    return true;
}

bool WebServer::serveStatic(int clientSock, const std::string& requestPath) {
    std::string decodedPath = QByteArray::fromPercentEncoding(QByteArray::fromStdString(requestPath)).toStdString();
    if (decodedPath.empty() || decodedPath.front() != '/') {
        sendJson(clientSock, 400, problem(400, "Invalid path"));
        return true;
    }

    std::filesystem::path relativePath(decodedPath.substr(1));
    for (const auto& component : relativePath) {
        if (component == "..") {
            sendJson(clientSock, 404, problem(404, "Not found"));
            return true;
        }
    }
    if (relativePath.empty()) {
        relativePath = "index.html";
    }

    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(m_webRoot, error);
    if (error || !std::filesystem::is_directory(root)) {
        sendJson(clientSock, 404, problem(404, "Frontend not found", "Build the web frontend before serving the UI."));
        return true;
    }
    auto filePath = std::filesystem::weakly_canonical(root / relativePath, error);
    if (error) {
        sendJson(clientSock, 404, problem(404, "Not found"));
        return true;
    }
    auto rootIt = root.begin();
    auto fileIt = filePath.begin();
    for (; rootIt != root.end() && fileIt != filePath.end(); ++rootIt, ++fileIt) {
        if (*rootIt != *fileIt) {
            sendJson(clientSock, 404, problem(404, "Not found"));
            return true;
        }
    }
    if (rootIt != root.end()) {
        sendJson(clientSock, 404, problem(404, "Not found"));
        return true;
    }

    if (!std::filesystem::is_regular_file(filePath)) {
        if (filePath.has_extension()) {
            sendJson(clientSock, 404, problem(404, "Not found"));
            return true;
        }
        filePath = root / "index.html";
    }
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        sendJson(clientSock, 404, problem(404, "Not found"));
        return true;
    }
    const std::string body((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    sendResponse(clientSock, 200, "OK", body, contentTypeForPath(filePath));
    return true;
}

void WebServer::sendResponse(int clientSock,
                             int statusCode,
                             const std::string& status,
                             const std::string& body,
                             const std::string& contentType,
                             bool cors,
                             const std::string& location) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << status << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n";
    if (cors) {
        response << "Access-Control-Allow-Origin: *\r\n"
                 << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 << "Access-Control-Allow-Headers: Content-Type\r\n";
    }
    if (!location.empty()) {
        response << "Location: " << location << "\r\n";
    }
    response << "\r\n" << body;
    sendAll(clientSock, response.str());
}

void WebServer::sendJson(int clientSock, int statusCode, const QJsonObject& body) {
    const std::string serialized = QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();
    const std::string contentType = body.contains("title") && body.contains("status")
        ? "application/problem+json; charset=utf-8"
        : "application/json; charset=utf-8";
    sendResponse(clientSock, statusCode, statusText(statusCode), serialized, contentType);
}
