#pragma once

#include "MessageQueueClient.h"

#include <string>

constexpr size_t HEADER_PACKET_SIZE = 6;
constexpr uint32_t MAX_PAYLOAD = 10 * 1024 * 1024;

inline std::map<std::string, char> client_role_map = {
    {"PUBLISHER", 'P'},
    {"SUBSCRIBER", 'S'}};

inline std::map<std::string, char> client_action_map = {
    {"CREATE_QUEUE", 'C'},
    {"DELETE_QUEUE", 'D'},
    {"PUBLISH", 'B'},
    {"SUBSCRIBE", 'S'},
    {"UNSUBSCRIBE", 'U'}};

// @brief Handling message Protocol.
//
// Header (6 bytes):
// @param Type: 2 ASCII bytes (e.g., "PB")
// @param Length: 4-byte uint32 network byte order (payload length N)
// @param Payload (N bytes) — type-dependent.
//
// Payload might be dependent on type of message.
//
// Publish payload:
// @param uint32 queue_name_len (network order)
// @param uint32 ttl_seconds (network order)
// @param queue_name (queue_name_len bytes)
// @param message (remaining bytes)
class Protocol {
public:
    static std::string prepare_message(char role, char cmd, const std::string &payload);

 private:
    static std::string _pack_publish_data(const std::string &queue_name, const std::string &content, const int ttl);
    static std::tuple<char, char, uint32_t> _decode_packet(const std::string &message);

    friend class MessageQueueClient;
};