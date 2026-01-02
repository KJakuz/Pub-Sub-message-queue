#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "common.h"


std::tuple<int, std::string, std::string> recv_message(int sock);
Client get_client_id(Client client);
std::string prepare_message(const std::string &message_type, const std::string &payload);
bool send_message(int sock, const std::string &data);

#endif