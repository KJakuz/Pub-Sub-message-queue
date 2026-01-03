/**
 * @file message_operations.h
 * @brief Queue and message operations.
 */
#ifndef MESSAGE_OPERATIONS_H
#define MESSAGE_OPERATIONS_H

#include "common.h"
#include "protocol_handler.h"

//helper functions
std::unordered_map<std::string, Queue>::iterator find_queue_by_name(const std::string& queue_name);
bool is_client_subscribed(const Queue& queue, const std::string& client_id);
std::vector<std::string>::iterator find_subscriber(Queue& queue, const std::string& client_id);
bool queue_exists(const std::string& queue_name);


//FUNCTIONS THAT RECEIVE DATA FROM CLIENT AND CHANGE QUEUES OR MESSAGES.
/**
 * @brief Subscribes client to queue.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name]
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of queue to subscribe to
 */
void subscribe_to_queue(const Client& client, const std::string& queue_name);

/**
 * @brief Unsubscribes client from queue.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name]
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of queue to unsubscribe from
 */
void unsubscribe_from_queue(const Client& client, const std::string& queue_name);

/**
 * @brief Creates new queue.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name]
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of queue to create
 */
void create_queue(const Client& client, const std::string& queue_name);

/**
 * @brief Deletes an existing queue.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name]
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of queue to delete
 */
void delete_queue(const Client& client, const std::string& queue_name);

/**
 * @brief Publishes message to queue.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name_size(4b)][ttl(4b)][queue_name][message]
 * 
 * @param client Client struct that contains client information
 * @param content Content of message to publish (includes queue name, TTL, and message data)
 */
void publish_message_to_queue(const Client& client, const std::string& content);

//Build queue list packet
std::string construct_queue_list();
//Broadcast queue list to all clients
void broadcast_queues_list();

//FUNCTIONS THAT ARE SENDING TO CLIENT DATA ABOUT QUEUES OR MESSAGES.
/**
 * @brief Sends list of all available queues to client.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queues_count(4b)][queue_name_size(4b)][queue_name][queue_name_size(4b)][queue_name]...
 * 
 * @param client Client struct that contains client information
 */
void send_single_queue_list(const Client& client);

/**
 * @brief Sends published message to a subscriber.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name_size(4b)][queue_name][message]
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of the queue the message was published to
 * @param content Content of the message to send
 */
void send_published_message(const Client& client, const std::string &queue_name, const std::string &content);

/**
 * @brief Notifies subscribers about queue deletion.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name_size(4b)][queue_name]
 * 
 * @param subscriber_ids Vector of subscriber clients IDs to notify
 */
void notify_after_delete(const std::vector<std::string>& subscriber_ids, const std::string& queue_name);

/**
 * @brief Sends all existing messages from a queue to a newly subscribed client.
 * 
 * Protocol format: [TYPE(2b)][SIZE(4b)][queue_name_size(4b)][queue_name][message_size(4b)][message][message_size(4b)][message]...
 * 
 * @param client Client struct that contains client information
 * @param queue_name Name of the queue to retrieve messages from
 */
void send_messages_to_new_subscriber(const Client& client, const std::string& queue_name);

#endif