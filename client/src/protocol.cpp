#include "Protocol.h"

std::string Protocol::_prepare_message(char role, char cmd, const std::string &payload) {
    std::string buf;
    buf.reserve(HEADER_PACKET_SIZE + payload.size());
    buf += role;
    buf += cmd;
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    buf.append(reinterpret_cast<const char *>(&len), sizeof(len));
    buf.append(payload);
    return buf;
}

std::string Protocol::_pack_publish_data(const std::string &queue_name, const std::string &content, const uint32_t ttl) {
    std::string internal_payload;
    internal_payload.reserve(sizeof(int) + sizeof(int) + content.size());

    uint32_t len_q = htonl(queue_name.size());
    internal_payload.append(reinterpret_cast<const char *>(&len_q), sizeof(len_q));
    uint32_t len_ttl = htonl(ttl);
    internal_payload.append(reinterpret_cast<const char *>(&len_ttl), sizeof(len_ttl));

    internal_payload += queue_name;
    internal_payload += content;

    return internal_payload;
}

std::tuple<char, char, uint32_t> Protocol::_decode_packet(const std::string &full_message) {
        if (full_message.size() < HEADER_PACKET_SIZE) {
            return {0, 0, 0};
        }

        char role = full_message[0];
        char cmd = full_message[1];

        uint32_t payload_len_net;
        std::memcpy(&payload_len_net, full_message.data() + 2, sizeof(uint32_t));
        uint32_t payload_len = ntohl(payload_len_net);

        return {role, cmd, payload_len};
}