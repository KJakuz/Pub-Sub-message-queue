#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>

#define PACKET_HEADER_SIZE 6

struct Queue {
    std::string name;
    std::string message;
    int ttl;
};

struct Client {
    std::string id;
    int socket;
    std::string type;
    std::vector<Queue> subscribed_queues;
    std::vector<Queue> published_queues;
};


extern std::unordered_map<std::string,Client> clients;
extern std::mutex clients_mutex;
extern std::mutex queues_mutex;

#endif