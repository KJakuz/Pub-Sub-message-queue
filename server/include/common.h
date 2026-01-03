/**
 * @file common.h
 * @brief Common definitions, data structures and global variables.
 */

#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <tuple>
#include <map>


//configuration
constexpr size_t PACKET_HEADER_SIZE = 6; //packet header size in bytes
constexpr int MAX_PAYLOAD_SIZE_MB = 10; //max payload size in MB
constexpr int SECONDS_TO_CLEAR_CLIENT = 30; //seconds to clear client after disconnection
constexpr int CLIENT_READ_TIMEOUT = 45; //client read timeout in seconds
constexpr int HEARTBEAT_INTERVAL = 30; //heartbeat/worker thread interval in seconds
constexpr int DEBUG = 0; //debug mode
constexpr int LOGS = 1; //logs mode

// Single message in a queue
struct Message {
    std::string text;
    std::chrono::steady_clock::time_point expire;
};

// Message queue
struct Queue {
    std::string name;
    std::vector<Message> messages;
    std::vector<std::string> subscribers;
    int ttl = 60;
};

// Connected client
struct Client {
    std::string id;
    int socket = -1;
    std::chrono::steady_clock::time_point disconnect_time;
};

// Protocol message types
enum class message_type {
    LOGIN,                     // LO
    SUBSCRIBE,                 // SS
    UNSUBSCRIBE,               // SU
    QUEUE_CREATE,              // PC
    QUEUE_DELETE,              // PD
    PUBLISH,                   // PB
    HEARTBEAT,                 // HB
    QUEUE_LIST,                // QL
    MESSAGE_MULTICAST,         // MS
    MESSAGE_TO_NEW_SUBSCRIBER, // MA
    QUEUE_DELETED_INFO,        // ND
    ERROR                      // ER
};

// String to enum mapping
inline const std::unordered_map<std::string, message_type> STR_TO_MSG_TYPE = {
    {"LO", message_type::LOGIN},
    {"SS", message_type::SUBSCRIBE},
    {"SU", message_type::UNSUBSCRIBE},
    {"PC", message_type::QUEUE_CREATE},
    {"PD", message_type::QUEUE_DELETE},
    {"PB", message_type::PUBLISH},
    {"HB", message_type::HEARTBEAT},
    {"QL", message_type::QUEUE_LIST},
    {"MS", message_type::MESSAGE_MULTICAST},
    {"MA", message_type::MESSAGE_TO_NEW_SUBSCRIBER},
    {"ND", message_type::QUEUE_DELETED_INFO},
    {"ER", message_type::ERROR}
};

// Enum to string mapping
inline const std::unordered_map<message_type, std::string> MSG_TYPE_TO_STR = {
    {message_type::LOGIN, "LO"},
    {message_type::SUBSCRIBE, "SS"},
    {message_type::UNSUBSCRIBE, "SU"},
    {message_type::QUEUE_CREATE, "PC"},
    {message_type::QUEUE_DELETE, "PD"},
    {message_type::PUBLISH, "PB"},
    {message_type::HEARTBEAT, "HB"},
    {message_type::QUEUE_LIST, "QL"},
    {message_type::MESSAGE_MULTICAST, "MS"},
    {message_type::MESSAGE_TO_NEW_SUBSCRIBER, "MA"},
    {message_type::QUEUE_DELETED_INFO, "ND"},
    {message_type::ERROR, "ER"}
};

//global variables
extern std::unordered_map<std::string, Queue> existing_queues;
extern std::unordered_map<std::string, Client> clients;

//LOCKS ALWAYS IN THE SAME ORDER -> QUEUES_MUTEX THEN CLIENTS_MUTEX
extern std::mutex clients_mutex;
extern std::mutex queues_mutex;

//other locks we dont use together
extern std::mutex log_mutex;
extern std::mutex socket_map_mutex;
extern std::map<int, std::mutex> socket_mutexes;

//thread safe print functions
void safe_print(const std::string& msg);
void safe_error(const std::string& msg);

#endif