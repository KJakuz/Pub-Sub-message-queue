#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "common.h"
#include <tuple>

Client get_client_id(Client client);
std::tuple<bool, std::string, std::string> validate_message(char buffer[], int bytes_received);

#endif