#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include "common.h"
#include "protocol_handler.h"

Client get_client_id(Client client, std::string &id);

extern std::mutex clients_mutex;
extern std::mutex queues_mutex;
extern std::unordered_map<std::string, Queue> existing_queues; 

#endif

