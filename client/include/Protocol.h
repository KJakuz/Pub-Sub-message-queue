#pragma once

#include "MessageQueueClient.h"

#include <string>

const int PACKET_LENGTH = 6;
inline std::map<std::string, char> ClientMode = {
    {"PUBLISHER", 'P'},
    {"SUBSCRIBER", 'S'}};

inline std::map<std::string, char> ClientActions = {
    {"CREATE_QUEUE", 'C'},
    {"DELETE_QUEUE", 'D'},
    {"PUBLISH", 'B'},
    {"SUBSCRIBE", 'S'},
    {"UNSUBSCRIBE", 'U'}};

class Protocol 
{
    friend class MessageQueueClient;
public:
    static std::string prepare_message(char role, char cmd, const std::string &payload);
private:
    static std::string _pack_publish_data(const std::string &queue_name, const std::string &content, const int ttl);
    static std::tuple<char, char, uint32_t> _decode_packet(const std::string &message);
};