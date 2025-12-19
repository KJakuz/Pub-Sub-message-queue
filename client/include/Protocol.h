#pragma once

#include <string>
#include "MessageQueueClient.h"

const int PACKET_LENGTH = 6;

class Protocol 
{
    friend class MessageQueueClient;
public:
    static std::string prepare_message(char role, char cmd, const std::string &payload);
private:
    static std::string _pack_publish_data(const std::string &queue_name, const std::string &content, const int ttl);
};