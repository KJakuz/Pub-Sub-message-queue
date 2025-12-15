#include "server_client_operations.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <csignal>
#include <atomic>

std::atomic<bool> running(true);
int listening_socket_global;

std::unordered_set<std::string> active_client_ids;
std::mutex clients_mutex;
std::mutex queues_mutex;

char messages_simple[1024];
int messages_simple_size = 0;

void signal_handler(int signal){
    if(signal == SIGINT){
        std::cout<<"\nShutting down server...\n";
        running = false;
        if(listening_socket_global != -1){
            shutdown(listening_socket_global, SHUT_RDWR);
            close(listening_socket_global);
        }
    }
}

void handle_client(int client_socket){
    //GET CLIENT INFO AND VALIDATE ID
    Client client;
    client.socket_fd = client_socket;
    client.id = get_client_id(client_socket);
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        if(active_client_ids.find(client.id) != active_client_ids.end()){
            perror("client already exists");
            close(client_socket);
            return;
        }
        
        active_client_ids.insert(client.id);
    }

    if(client.id.empty() ){
        perror("client id error");
        close(client_socket);
        return;
    }
    std::cout << "Client " << client.id <<" connected"<<"\n";

    //CLIENT OPERATIONS
    while(true){
        //GET CLIENT TYPE
        client.type = get_client_type(client_socket);
        if(client.type == "error"){
            perror("uncorrect client type");
            continue;
        }
        else if(client.type == "disconnected"){
            break;
        }
        else if(client.type == "sub"){
            //SUBSCRIBE QUEUE
            //READ SUBSCRIBED QUEUES
            std::cout<<"client "<<client.id<<" type: "<<client.type<<"\n";
            //send_messages(client_socket, client.subscribed_queues);
            if (messages_simple_size > 0){
                send(client_socket, messages_simple, messages_simple_size, 0);
                std::cout<<messages_simple<<" was sent to "<<client.id<<"\n";
            }
        }
        else if(client.type == "pub"){
            //CREATE NEW QUEUE
            //DELETE QUEUE
            //SEND MESSAGE TO QUEUE

            std::cout<<"client "<<client.id<<" type: "<<client.type<<"\n";
            messages_simple_size = recv(client_socket, messages_simple, sizeof(messages_simple), 0);
            messages_simple[messages_simple_size - 1] = '\0';
            std::cout<<messages_simple<<" was received from "<<client.id<<"\n";
            
        }

        if (recv(client_socket, NULL, 0, 0) == 0){
            break;
        }
    }

    //DISCONNECT CLIENT WHEN CLIENT LEAVES AND CLEAR DATA
    std::cout<<"client "<<client.id<<" disconnected\n";
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        active_client_ids.erase(client.id);
    }
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    return;
}

std::vector<Client> clients;

int main(int argc, char  **argv){

    signal(SIGINT, signal_handler);

    int listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listening_socket == -1){
        perror("socket error");
        return -1;
    }
    listening_socket_global = listening_socket;

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(3333);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    int opt = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    if(bind(listening_socket, (sockaddr*) &server_address, sizeof(server_address)) == -1){
        perror("bind error");
        close(listening_socket);
        return -1;
    }


    if (listen(listening_socket, SOMAXCONN) == -1){
        perror("listen error");
        close(listening_socket);
        return -1;
    }

    while(running){ 
        int client_socket = accept(listening_socket, NULL, NULL);
        if(client_socket == -1){
            if(running){
                perror("accept error");
                break;
            }
        }
        std::thread t(handle_client, client_socket);
        t.detach();
    }

    //DISCONNECT CLIENTS WHEN SERVER IS SHUT DOWN
    shutdown(listening_socket, SHUT_RDWR);
    close(listening_socket);
    std::cout << "Server cleanup complete\n";

    return 0;
}