#ifndef MESSAGE_OPERATIONS_H
#define MESSAGE_OPERATIONS_H

#include "common.h"
#include "protocol_handler.h"

std::vector<Queue>::iterator find_queue_by_name(const std::string& queue_name);
bool is_client_subscribed(const Queue& queue, const std::string& client_id);
std::vector<std::string>::iterator find_subscriber(Queue& queue, const std::string& client_id);
bool queue_exists(const std::string& queue_name);

void subscribe_to_queue(Client client, std::string queue_name); //RECV QUEUE NAME TO SUBSCRIBE: [SS SIZE QUEUE_NAME]
void unsubscribe_from_queue(Client client, std::string queue_name); //RECV QUEUE NAME TO UNSUBSCRIBE FROM: [SU SIZE QUEUE_NAME]
void create_queue(Client client, std::string queue_name); //RECV QUEUE NAME TO CREATE: [PC SIZE QUEUE_NAME]
void delete_queue(Client client, std::string queue_name); //RECV QUEUE NAME TO DELETE: [PD SIZE QUEUE_NAME]
void publish_message_to_queue(Client client, std::string content); //RECV SINGLE MESSAGE TO INSERT INTO QUEUE: [PB SIZE QUEUE_NAME]

std::string construct_queue_list(); //PREPARES MESSAGE FOR BROADCAST QUEUES LIST AND SEND SINGLE QUEUE LIST
void send_single_queue_list(Client client); //SEND QUEUES LIST: [QL SIZE QUEUE_SIZE1 QUEUE_NAME1 ... QUEUE_SIZEn QUEUE_NAMEn]
void broadcast_queues_list(); //SEND QUEUES LIST: [QL SIZE QUEUE_SIZE1 QUEUE_NAME1 ... QUEUE_SIZEn QUEUE_NAMEn]
void notify_after_delete(std::vector<std::string>); //SEND NOTIFY AFTER DELETE: [ND SIZE QUEUE_X_WAS_DELETED_YOU_WERE_UNSUBSCRIBED_AUTOMATICALLY]
void send_published_message(Client client,std::string &queue_name, std::string &content); //SEND SINGLE MESSAGE: [MS SIZE QUEUE_NAME_SIZE QUEUE_NAME CONTENT]
void send_messages_to_new_subscriber(Client client, std::string queue_name); //SENDS ALL ACTIVE MESSAGES FROM QUEUE: 
                                                                            //[MA SIZE QUEUE_NAME_SIZE QUEUE_NAME MESSAGE_SIZE1 MESSAGE1 ... MESSAGE_SIZEn MESSAGEn]



extern std::mutex clients_mutex;
extern std::mutex queues_mutex;
extern std::vector<Queue> Existing_Queues; 

#endif

