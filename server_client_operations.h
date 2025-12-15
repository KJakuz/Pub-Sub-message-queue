#ifndef SERVER_CLIENT_OPERATIONS_H
#define SERVER_CLIENT_OPERATIONS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>

struct Queue{
    std::string name;
    std::string message;
    int ttl;
};

struct Client{
    std::string id;
    int socket_fd;
    std::string type;
    std::vector<Queue> subscribed_queues;
    std::vector<Queue> published_queues;
    //std::chrono disconnect_time;
};

struct Message{
    std::string content;
    //std::chrono::time_point ttl;
};

std::string get_client_id(int client_socket){
    char buffer[16];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if(bytes_received == -1){
        perror("recv error");
        return "";
    }
    std::string client_id = std::string(buffer, bytes_received);
    client_id.erase(client_id.find_last_not_of(" \n\r\t") + 1);
    return client_id;
}

std::string get_client_type(int client_socket){
    char buffer[128] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if(bytes_received == -1){
        perror("recv error in get_client_type");
        return "error";
    }
    buffer[bytes_received] = '\0';
    if(buffer[0] == 'p' && buffer[1] == 'u' && buffer[2] == 'b'){
        return "pub";
    }
    else if(buffer[0] == 's' && buffer[1] == 'u' && buffer[2] == 'b'){
        return "sub";
    }
    else if(bytes_received == 0){
        return "disconnected";
    }
    return "error";
}

void send_messages(int client_socket, std::vector<Queue> messages){
    char buffer[1024];
    while(messages.size() > 0){
        
    }
    int bytes_received = send(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    std::cout<<buffer<<std::endl;    
}

#endif