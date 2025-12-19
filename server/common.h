#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <cstring>

#define PACKET_HEADER_SIZE 6
#define WITH_ENDLINES 0

struct Queue {
    std::string name;
    std::vector<std::string> messages;
    std::vector<std::string> subscribers;
    int ttl;
};

struct Client {
    std::string id;
    int socket;
    std::string type;
};

extern std::vector<Queue> Existing_Queues; 
extern std::unordered_map<std::string,Client> clients;
extern std::mutex clients_mutex;
extern std::mutex queues_mutex;

#endif