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

#define PACKET_HEADER_SIZE 6
#define MAX_PAYLOAD_SIZE_MB 10
#define SECONDS_TO_CLEAR_CLIENT 30
#define CLIENT_READ_TIMEOUT 45
#define HEARTBEAT_INTERVAL 30
#define DEBUG 0
#define LOGS 1

struct Message {
    std::string text;
    std::chrono::steady_clock::time_point expire;
};

struct Queue {
    std::string name;
    std::vector<Message> messages;
    std::vector<std::string> subscribers;
    int ttl = 60;
};

struct Client {
    std::string id;
    int socket = -1;
    std::chrono::steady_clock::time_point disconnect_time;
};


extern std::unordered_map<std::string, Queue> existing_queues; 
extern std::unordered_map<std::string,Client> clients;
extern std::mutex clients_mutex;
extern std::mutex queues_mutex;
extern std::mutex log_mutex;
extern std::mutex socket_map_mutex;
extern std::map<int, std::mutex> socket_mutexes;

void safe_print(const std::string& msg);
void safe_error(const std::string& msg);

#endif