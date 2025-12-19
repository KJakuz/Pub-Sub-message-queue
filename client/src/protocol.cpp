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

std::string Protocol::_pack_publish_data(const std::string &queue_name, const std::string &content, const int ttl) {
    std::string internal_payload;
    internal_payload.reserve(sizeof(int) + sizeof(int) + content.size());

    uint32_t len_q = htonl(queue_name.size());
    internal_payload.append(reinterpret_cast<const char *>(&len_q), sizeof(len_q));
    uint32_t len_ttl = htonl(ttl);
    internal_payload.append(reinterpret_cast<const char *>(&len_ttl), sizeof(len_ttl    ));

    internal_payload += queue_name;
    internal_payload += content;

    return internal_payload;
}