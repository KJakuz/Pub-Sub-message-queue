#include "message_operations.h"
#include <sys/socket.h>
#include <iomanip>
#include <sstream>
#include <iostream>

void send_messages_to_subscriber(int client_socket) {  

    return;
}

void subscribe_to_queue(int client_socket, std::string queue_name) {
    std::cout<<"stq\n";
    return;
}

void unsubscribe_from_queue(int client_socket, std::string queue_name) {  
    std::cout<<"ufq\n";
    return;
}

void create_queue(int client_socket, std::string queue_name) {
    std::cout<<"cq\n";
    return;
}

void delete_queue(int client_socket, std::string queue_name) {
    std::cout<<"dq\n";
    return;
}

void publish_message_to_queue(int client_socket, std::string content) {
    std::cout<<"pmtq\n";
    return;
}