#ifndef MESSAGE_OPERATIONS_H
#define MESSAGE_OPERATIONS_H

#include "common.h"
#include <mutex>

void send_messages_to_subscriber(int client_socket);
void subscribe_to_queue(int client_socket, std::string queue_name);
void unsubscribe_from_queue(int client_socket, std::string queue_name);
void create_queue(int client_socket, std::string queue_name);
void delete_queue(int client_socket, std::string queue_name);
void publish_message_to_queue(int client_socket, std::string content);

extern std::mutex clients_mutex;

#endif

