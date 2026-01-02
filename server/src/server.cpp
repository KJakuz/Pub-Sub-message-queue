#include "protocol_handler.h"
#include "message_operations.h"
#include "client_operations.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <netdb.h>
#include <algorithm>
#include <list>

std::atomic<bool> running(true);
std::atomic<int> listening_socket_global(-1);

std::mutex clients_mutex;
std::mutex queues_mutex;
std::mutex log_mutex;
std::mutex threads_mutex;
std::mutex socket_map_mutex;
std::map<int, std::mutex> socket_mutexes;

std::unordered_map<std::string, Client> clients;
std::unordered_map<std::string, Queue> existing_queues;
std::list<std::thread> client_threads;


void safe_print(const std::string& msg) {
    if(LOGS){
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << msg << std::endl;
    }
}

void safe_error(const std::string& msg) {
    if(LOGS){
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << msg << std::endl;
    }
}

void cleanup_worker() {
    int heartbeat_counter = 0;
    while (running) {

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running) break;
        
        heartbeat_counter++;
        if (heartbeat_counter < HEARTBEAT_INTERVAL) {
            continue;
        }
        else{
            heartbeat_counter = 0;   
        }

        auto now = std::chrono::steady_clock::now();
        std::vector<int> alive_sockets;

        //clients cleanup and getting alive sockets
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto it = clients.begin(); it != clients.end(); ) {
                if (it->second.socket == -1) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        now - it->second.disconnect_time).count();
                    if (elapsed >= SECONDS_TO_CLEAR_CLIENT) {
                        safe_print("Removing expired client: " + it->first);
                        it = clients.erase(it);
                        continue;
                    }
                }
                else{
                    alive_sockets.push_back(it->second.socket);
                }
                ++it;
            }
        }
        
        //send heartbeat to alive clients
        for (int socket : alive_sockets) {
            send_message(socket, prepare_message("HB", ""));
        }
        
        //messages cleanup after ttl expire
        {
            std::lock_guard<std::mutex> lock(queues_mutex);
            for (auto& [name, queue] : existing_queues) {
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
        safe_print("\nShutting down server...");
        running = false;
        if(listening_socket_global != -1){
            shutdown(listening_socket_global, SHUT_RDWR);
            close(listening_socket_global);
        }
    }
}


void handle_client(int client_socket){
    //timeout after CLIENT_READ_TIMEOUT seconds of no activity
    struct timeval tv;
    tv.tv_sec = CLIENT_READ_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        safe_error("setsockopt SO_RCVTIMEO failed");
        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
        return;
    }

    Client client;
    client.socket = client_socket;

    while(running){
        recv_status status;
        std::string msg_type, msg_content;
        std::tie(status, msg_type, msg_content) = recv_message(client_socket);
        
        if (status == recv_status::DISCONNECT) {
            safe_print("socket:"+ std::to_string(client.socket) +"  client id:"+ (client.id.empty() ? "Unknown" : client.id) + "  disconnected");
            break;
        }
        else if (status == recv_status::NETWORK_ERROR) {
            if (errno == ECONNRESET) {
                 safe_print((client.id.empty() ? "Unknown" : client.id) + " disconnected abruptly (ECONNRESET)");
            } else {
                 safe_error("recv error from socket " + std::to_string(client_socket) + " (errno=" + std::to_string(errno) + ")");
            }
            break;
        }
        else if (status == recv_status::PAYLOAD_TOO_LARGE) {
             safe_error("Client " + (client.id.empty() ? "Unknown" : client.id) + " tried to send too huge message");
             send_message(client_socket, prepare_message(msg_type, "ER:MSG_TOO_BIG"));
             break;
        }
        else if (status == recv_status::PROTOCOL_ERROR) {
            safe_error("ERROR MESSAGE NOT VALID FROM SOCKET:" + std::to_string(client.socket));
            continue;
        }
        
        //if client not logged in yet
        if(client.id.empty()){
            if(msg_type == "LO"){
                client.socket = client_socket;
                client = get_client_id(client, msg_content);
                send_single_queue_list(client);
            }
            else{
                if(!send_message(client.socket, prepare_message("LO","ER:FIRST YOU MUST LOG IN"))){
                    safe_error("ERROR SENDING MESSAGE LO:ER TO SOCKET:" + std::to_string(client.socket));
                }
            }
        }
        else //client logged in
        {
            if(msg_type == "HB"){ //sends heartbeat to client
                continue;
            }
            else if(msg_type == "SS"){
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
                    safe_error("ERROR SENDING MESSAGE LO:ER TO " + client.id);
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
            safe_print("Client " + client.id + " disconnected (session preserved)");
        }
    }

    {
        std::lock_guard<std::mutex> lock(socket_map_mutex);
        socket_mutexes.erase(client_socket);
    }
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    return;
}


int main(int argc, char  **argv){
    
    if (argc < 2) {
        safe_error("Usage: " + std::string(argv[0]) + " <port>");
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int gai_err = getaddrinfo(NULL, argv[1], &hints, &res);
    if (gai_err != 0) {
        safe_error("getaddrinfo error: " + std::string(gai_strerror(gai_err)));
        freeaddrinfo(res);
        return -1;
    }

    int listening_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listening_socket == -1) {
        safe_error("socket creation failed");
        freeaddrinfo(res);
        return -1;
    }
    listening_socket_global = listening_socket;

    int opt = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listening_socket, res->ai_addr, res->ai_addrlen) == -1) {
        safe_error("bind failed");
        close(listening_socket);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (listen(listening_socket, SOMAXCONN) == -1) {
        safe_error("listen failed");
        close(listening_socket);
        return -1;
    }

    // Start worker thread
    std::thread worker(cleanup_worker);

    //accept clients
    while(running){ 
        int client_socket = accept(listening_socket, NULL, NULL);
        if(client_socket == -1){
            if(running){
                safe_error("accept failed");
            }
            break;
        }
        std::lock_guard<std::mutex> lock(threads_mutex);
        client_threads.emplace_back(handle_client, client_socket);
    }

    //cleanup clients, join threads, close listening socket
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto& [id, client] : clients){
            if(client.socket != -1) {
                shutdown(client.socket, SHUT_RDWR);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        for(auto& t : client_threads){
            if(t.joinable()) t.join();
        }
        client_threads.clear();
    }

    worker.join();

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto& [id, client] : clients){
            if(client.socket != -1) {
                close(client.socket);
            }
        }
        clients.clear();
    }

    safe_print("Server cleanup complete");
    return 0;
}