#include "protocol_handler.h"
#include "message_operations.h"
#include "client_operations.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <tuple>
#include <unistd.h>

//TODO:WORKER CLEANUP THREAD / WYSLANIE PRZY LOGOWANIU LISTY KOLEJEK

std::atomic<bool> running(true);
int listening_socket_global;

std::mutex clients_mutex;
std::mutex queues_mutex;
std::unordered_map<std::string, Client> clients;
std::vector<Queue> Existing_Queues;


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

    //CLIENT OPERATIONS
    while(running){
        
        //CHECK WHAT MESSAGE IS BEING SENT
        char buffer[8192] = {};     //TODO: HOW BIG BUFFER (PROTOCOL ALLOWS CONTENT TO BE 4GB)
        int bytes_received = 0;

            //get message sender,type,size and content
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if(bytes_received == -1){
            perror("recv error");
            break;
        }

        bool valid;
        std::string msg_type;
        std::string msg_content;

        //DEBUG
        std::cout<<"DEBUG:\n";
        std::cout<<"QUEUES: [";
        for(size_t i = 0; i < Existing_Queues.size(); i++){
            std::cout<<Existing_Queues[i].name;
            if(i < Existing_Queues.size() - 1) std::cout<<", ";
        }
        std::cout<<"]\n";
        //DEBUG END


        std::tie(valid, msg_type, msg_content) = validate_message(buffer, bytes_received);

        if (!valid && msg_type == "d"){
            shutdown(client.socket, SHUT_RDWR);
            close(client.socket);
            break;
        }
        else if(!valid){
            std::cerr<<"ERROR MESSAGE NOT VALID FROM SOCKET:"<<client.socket<<"\n";
            continue;
        }

        if(client.id.empty()){
            if(msg_type == "LO"){
                client.socket = client_socket;
                client = get_client_id(client, msg_content);
                send_single_queue_list(client);
            }
            else{
                if(!send_message(client.socket, prepare_message("LO","ER:FIRST YOU MUST LOG IN"))){
                    std::cerr<<"ERROR SENDING MESSAGE LO:ER TO SOCKET:"<<client.socket<<"\n";
                }
            }
        }
        else
        {
            if(msg_type == "SS"){
                subscribe_to_queue(client, msg_content);
            }
            else if(msg_type == "SU"){
                unsubscribe_from_queue(client,msg_content);
            }
            else if(msg_type == "PC"){
                create_queue(client,msg_content);
            }
            else if(msg_type == "PD"){
                delete_queue(client,msg_content);
            }
            else if(msg_type == "PB"){
                publish_message_to_queue(client,msg_content);
            }
            else if(msg_type == "LO"){
                if(!send_message(client.socket, prepare_message("LO","ER:USER_ID_ALREADY_GIVEN"))){
                    std::cerr<<"ERROR SENDING MESSAGE LO:ER TO "<<client.id<<"\n";
                }
            }
        }
        
    }

    //NOTE THAT CLIENT DISCONECTED
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = clients.find(client.id);
        if (it != clients.end()) {
            it->second.disconnect_time = std::chrono::steady_clock::now();
            it->second.socket = -1;
            std::cout << "Client " << client.id << " disconnected (session preserved)\n";
        }
    }
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    return;
}


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
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto& [id, client] : clients){
            shutdown(client.socket, SHUT_RDWR);
            close(client.socket);
        }
        clients.clear();
    }

    std::cout << "Server cleanup complete\n"; //TODO: recv error: Bad file descriptor \n recv error: Bad file descriptor

    return 0;
}