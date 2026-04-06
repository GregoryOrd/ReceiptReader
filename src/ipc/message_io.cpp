#include "ipc/message_io.h"
#include <arpa/inet.h>
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

static bool recvAll(int sock, void* data, size_t size) {
    char* ptr = reinterpret_cast<char*>(data);
    while (size > 0) {
        ssize_t received = recv(sock, ptr, size, 0);
        if (received <= 0) {
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

bool receiveProtobufMessage(int sock, google::protobuf::MessageLite* message) {
    uint32_t networkLength;
    if (!recvAll(sock, &networkLength, sizeof(networkLength))) {
        return false;
    }
    uint32_t length = ntohl(networkLength);
    if (length == 0) {
        return message->ParseFromArray(nullptr, 0);
    }
    std::vector<char> buffer(length);
    if (!recvAll(sock, buffer.data(), length)) {
        return false;
    }
    return message->ParseFromArray(buffer.data(), length);
}

} // namespace ipc
