#include "ipc/message_io.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace ipc {

static bool sendAll(int sock, const void* data, size_t size) {
    const char* ptr = reinterpret_cast<const char*>(data);
    while (size > 0) {
        ssize_t sent = send(sock, ptr, size, 0);
        if (sent <= 0) {
            return false;
        }
        ptr += sent;
        size -= sent;
    }
    return true;
}

static bool recvAll(int sock, void* data, size_t size, const char* stage, std::string& error) {
    char* ptr = reinterpret_cast<char*>(data);
    while (size > 0) {
        ssize_t received = recv(sock, ptr, size, 0);
        if (received == 0) {
            error = std::string("Connection closed while receiving protobuf ") + stage + ".";
            return false;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = std::string("Failed to receive protobuf ") + stage + ": " + strerror(errno)
                    + " (errno " + std::to_string(errno) + ").";
            return false;
        }
        ptr += received;
        size -= received;
    }
    return true;
}

bool sendProtobufMessage(int sock, const google::protobuf::MessageLite& message) {
    std::string buffer;
    if (!message.SerializeToString(&buffer)) {
        return false;
    }

    uint32_t length = static_cast<uint32_t>(buffer.size());
    uint32_t networkLength = htonl(length);
    if (!sendAll(sock, &networkLength, sizeof(networkLength))) {
        return false;
    }
    return sendAll(sock, buffer.data(), buffer.size());
}

bool receiveProtobufMessage(int sock, google::protobuf::MessageLite* message, std::string& error) {
    error.clear();

    uint32_t networkLength;
    if (!recvAll(sock, &networkLength, sizeof(networkLength), "message length", error)) {
        return false;
    }
    uint32_t length = ntohl(networkLength);
    if (length == 0) {
        if (!message->ParseFromArray(nullptr, 0)) {
            error = "Failed to parse empty protobuf message.";
            return false;
        }
        return true;
    }
    std::vector<char> buffer(length);
    if (!recvAll(sock, buffer.data(), length, "message payload", error)) {
        return false;
    }
    if (!message->ParseFromArray(buffer.data(), length)) {
        error = "Failed to parse protobuf message.";
        return false;
    }
    return true;
}

} // namespace ipc
