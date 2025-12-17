#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>

#define PACKET_HEADER_SIZE 6

struct Queue {
    std::string name;
    std::string message;
    int ttl;
};

struct Client {
    std::string id;
    int socket_fd;
    std::string type;
    std::vector<Queue> subscribed_queues;
    std::vector<Queue> published_queues;
};

#endif