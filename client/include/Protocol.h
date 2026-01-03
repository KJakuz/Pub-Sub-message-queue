#pragma once

#include "MessageQueueClient.h"

#include <string>

constexpr size_t HEADER_PACKET_SIZE = 6;
constexpr uint32_t MAX_PAYLOAD = 1 * 1024 * 1024;

// @brief Message sender role.
namespace Role {
    inline constexpr char Publisher = 'P';
    inline constexpr char Subscriber = 'S';
}

// @brief Message action / command type.
namespace Action {
    inline constexpr char Create = 'C';
    inline constexpr char Delete = 'D';
    inline constexpr char Publish = 'B';
    inline constexpr char Subscribe = 'S';
    inline constexpr char Unsubscribe = 'U';
}

// All protocol messages consist of a fixed-size header followed
// by an optional payload.

// Message header (6 bytes):
// Offset | Size | Description
// -------|------|----------------------------------------------
// 0      | 1    | Role
// 1      | 1    | Action / Command
// 2      | 4    | Payload length (uint32, network byte order)
//
// The header is immediately followed by a payload of exactly
// `payload_length` bytes.

// Publish payload format:
// Offset | Size | Description
// -------|------|----------------------------------------------
// 0      | 4    | Queue name length (uint32, network byte order)
// 4      | 4    | Message TTL in seconds (uint32, network byte order)
// 8      | N    | Queue name (N bytes)
// 8 + N  | M    | Message content (remaining bytes)
//
// This class  is used internally by MessageQueueClient to construct
// and parse protocol-compliant messages.
class Protocol {
public:
private:
    // @brief Construct a full protocol message (header + payload).
    //
    // @param role Message sender role.
    // @param cmd Action / command code.
    // @param payload Payload data.
    //
    // @return Message ready to be sent over the socket.
    static std::string _prepare_message(char role, char cmd, const std::string &payload);

    // @brief Pack publish-specific payload data.
    //
    // @param queue_name Target queue name.
    // @param content Message body.
    // @param ttl Message time-to-live in seconds.
    //
    // @return Serialized publish payload.
    static std::string _pack_publish_data(const std::string &queue_name, const std::string &content, const uint32_t ttl);

    // @brief Decode a protocol header.
    //
    // @param message A buffer containing at least HEADER_PACKET_SIZE bytes.
    //
    // @return Tuple containing: role (char), action/command (char), payload length (uint32, host byte order).
    static std::tuple<char, char, uint32_t> _decode_packet(const std::string &message);

    friend class MessageQueueClient;
};