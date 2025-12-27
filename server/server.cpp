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
#include <netdb.h>

std::atomic<bool> running(true);
int listening_socket_global;

std::mutex clients_mutex;
std::mutex queues_mutex;
std::unordered_map<std::string, Client> clients;
std::vector<Queue> Existing_Queues;


void cleanup_worker() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (!running) break;
        
        auto now = std::chrono::steady_clock::now();
        
        //clients cleanup
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto it = clients.begin(); it != clients.end(); ) {
                if (it->second.socket == -1) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        now - it->second.disconnect_time).count();
                    if (elapsed >= SECONDS_TO_CLEAR_CLIENT) {
                        std::cout << "Removing expired client: " << it->first << "\n";
                        it = clients.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
        
        //messages cleanup
        {
            std::lock_guard<std::mutex> lock(queues_mutex);
            for (auto& queue : Existing_Queues) {
                auto& msgs = queue.messages;
                msgs.erase(
                    std::remove_if(msgs.begin(), msgs.end(),
                        [&now](const Message& m) { return now > m.expire; }),
                    msgs.end()
                );
            }
        }
    }
}


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
    Client client;

    while(running){
        //read message
        int status;
        std::string msg_type, msg_content;
        std::tie(status, msg_type, msg_content) = recv_message(client_socket);
        
        if (status == 0) {
            std::cout <<client.id<<"disconnected\n";
            break;
        }
        else if (status < 0 && msg_type.empty()) {
            perror("recv error");
            break;
        }
        else if (status < 0) {
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

    //client disconnected
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
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return -1;
    }

    signal(SIGINT, signal_handler);

    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int gai_err = getaddrinfo(NULL, argv[1], &hints, &res);
    if (gai_err != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(gai_err) << "\n";
        return -1;
    }

    int listening_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listening_socket == -1) {
        perror("socket error");
        freeaddrinfo(res);
        return -1;
    }
    listening_socket_global = listening_socket;

    int opt = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listening_socket, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind error");
        close(listening_socket);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (listen(listening_socket, SOMAXCONN) == -1) {
        perror("listen error");
        close(listening_socket);
        return -1;
    }

    // Start worker thread
    std::thread worker(cleanup_worker);

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

    // Stop worker thread
    worker.join();

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto& [id, client] : clients){
            shutdown(client.socket, SHUT_RDWR);
            close(client.socket);
        }
        clients.clear();
    }

    std::cout << "Server cleanup complete\n";
    return 0;
}