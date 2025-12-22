#ifndef MESSAGE_OPERATIONS_H
#define MESSAGE_OPERATIONS_H

#include "common.h"
#include "protocol_handler.h"

std::vector<Queue>::iterator find_queue_by_name(const std::string& queue_name);
bool is_client_subscribed(const Queue& queue, const std::string& client_id);
std::vector<std::string>::iterator find_subscriber(Queue& queue, const std::string& client_id);
bool queue_exists(const std::string& queue_name);

void send_messages_to_new_subscriber(Client client, std::string queue_name);
void subscribe_to_queue(Client client, std::string queue_name);
void unsubscribe_from_queue(Client client, std::string queue_name);
void create_queue(Client client, std::string queue_name);
void delete_queue(Client client, std::string queue_name);
void publish_message_to_queue(Client client, std::string content);
void broadcast_queues_list();
void send_published_message(Client client,std::string &queue_name, std::string &content);

extern std::mutex clients_mutex;
extern std::mutex queues_mutex;
extern std::vector<Queue> Existing_Queues; 

#endif

