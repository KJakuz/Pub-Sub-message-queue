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

#define PACKET_HEADER_SIZE 6
#define WITH_ENDLINES 0
#define SECONDS_TO_CLEAR_CLIENT 30
#define CLIENT_READ_TIMEOUT 60

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


extern std::unordered_map<std::string, Queue> Existing_Queues; 
extern std::unordered_map<std::string,Client> clients;
extern std::mutex clients_mutex;
extern std::mutex queues_mutex;
extern std::mutex log_mutex;

void safe_print(const std::string& msg);
void safe_error(const std::string& msg);

#endif