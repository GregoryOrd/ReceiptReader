#ifndef MESSAGE_IO_H
#define MESSAGE_IO_H

#include <google/protobuf/message_lite.h>

namespace ipc {

bool sendProtobufMessage(int sock, const google::protobuf::MessageLite& message);
bool receiveProtobufMessage(int sock, google::protobuf::MessageLite* message);

} // namespace ipc

#endif // MESSAGE_IO_H
