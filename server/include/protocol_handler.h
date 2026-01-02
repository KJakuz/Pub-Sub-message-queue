#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "common.h"


enum class recv_status {
    SUCCESS,
    DISCONNECT,
    NETWORK_ERROR,
    PROTOCOL_ERROR,
    PAYLOAD_TOO_LARGE
};

std::tuple<recv_status, std::string, std::string> recv_message(int sock);
Client get_client_id(Client client);
std::string prepare_message(const std::string &message_type, const std::string &payload);
bool send_message(int sock, const std::string &data);



#endif