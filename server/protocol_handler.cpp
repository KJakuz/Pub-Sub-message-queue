#include "protocol_handler.h"
#include <sys/socket.h>
#include <iostream>
#include <charconv>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

Client get_client_id(Client client){
    while(true){
        char buffer[1024] = {};
        int bytes_received = recv(client.socket, buffer, sizeof(buffer), 0);

        bool valid;
        std::string msg_type;
        std::string id;
        std::tie(valid, msg_type, id) = validate_message(buffer, bytes_received);
        if (!valid && msg_type == "d"){
            shutdown(client.socket, SHUT_RDWR);
            close(client.socket);
            return client;
        }
        else if(!valid){
            perror("message not valid");
            continue;
        }

        if(msg_type == "LO"){
            id.erase(id.find_last_not_of(" \n\r\t") + 1);

            if (id.length() < 2){
                std::string msg = prepare_message("ER","ID_SHORT");
                if(!send_message(client.socket, msg)){
                    perror("couldnt send message to client: get id lenght error");
                }
                continue;
            }
            
            bool id_taken = false;
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                
                if(clients.find(id) != clients.end()){
                    id_taken = true;
                } else {
                    client.id = id;
                    clients[id] = client;
                }
            }
            
            if(id_taken){
                std::string msg = prepare_message("ER","ID_TAKEN");
                if(!send_message(client.socket, msg)){
                    perror("couldnt send message to client: get id is taken");
                }
                continue;
            }
            client.id = id;
            std::string msg = prepare_message("OK","");
            if(!send_message(client.socket, msg)){
                perror("couldnt send message to client: get id OK");
            }
            std::cout << "Client " << client.id << " connected\n";
            break;
        }
    }
    return client;
}

std::tuple<bool, std::string, std::string> validate_message(char buffer[], int bytes_received){
    if (bytes_received < PACKET_HEADER_SIZE) return {false, "d", ""};
    
    uint32_t network_len;
    std::memcpy(&network_len, buffer + 2, sizeof(uint32_t));
    uint32_t content_size = ntohl(network_len);

    if( content_size + WITH_ENDLINES != (uint32_t)bytes_received - PACKET_HEADER_SIZE){
        return {false, "", ""};
    }

    std::string message_type;
    message_type += buffer[0];
    message_type += buffer[1];

    std::string content(buffer + 6, content_size);

    std::cout << "DEBUG: \nTYPE: " << message_type << "      SIZE: " << content_size <<"     CONTENT: "<<content<<"\n";

    if (message_type == "LO"||message_type == "SM" || message_type == "SS" || message_type == "SU" || message_type == "PC" || message_type == "PD" || message_type == "PB"){
        return {true, message_type, content};
        }
    else{
        return {false, "", ""};
    }
}

std::string prepare_message(const std::string &message_type, const std::string &payload) {
    std::string buf;
    buf.reserve(PACKET_HEADER_SIZE + payload.size());
    buf += message_type;
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    buf.append(reinterpret_cast<const char *>(&len), sizeof(len));
    buf.append(payload);
    return buf;
}

bool send_message(int sock, const std::string &data) {
    if (sock == -1) return false;

    size_t total_sent = 0;
    size_t data_len = data.size();
    const char *raw_data = data.data();

    while (total_sent < data_len) {
        ssize_t sent = send(sock, raw_data + total_sent, data_len - total_sent, 0);
        if (sent <= 0) { 
            return false;
        }
        total_sent += sent;
    }
    return true;
}