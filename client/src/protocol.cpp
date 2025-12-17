#include "Protocol.h"

std::string Protocol::prepare_message(char role, char cmd, const std::string &payload) {
    std::string buf;
    buf.reserve(PACKET_LENGTH + payload.size());
    buf += role;
    buf += cmd;
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    buf.append(reinterpret_cast<const char *>(&len), sizeof(len));
    buf.append(payload);
    return buf;
}