#include "protocol_handler.h"
#include <sys/socket.h>
#include <iostream>
#include <charconv>
#include <cstring>

Client get_client_id(Client client){
    while(true){
        char buffer[16] = {};
        int bytes_received = recv(client.socket, buffer, sizeof(buffer), 0);
        if(bytes_received <= 0){
            perror("recv error");
            return client;
        }
        
        std::string id(buffer, bytes_received);
        id.erase(id.find_last_not_of(" \n\r\t") + 1);

        if (id.length() < 2){
            const char* answer = "id too short\n"; //TODO PROTOKOL SERWER -> KLIENT
            send(client.socket, answer, strlen(answer), 0);
            continue;
        }
        
        bool id_taken = false;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            
            if(clients.find(id) != clients.end()){
                id_taken = true;
            } else {
                client.id = id;
                clients[id] = client;  // Dodaj klienta do mapy
            }
        }
        
        if(id_taken){
            const char* answer = "id already used\n";
            send(client.socket, answer, strlen(answer), 0);
            continue;
        }
        client.id = id;
        const char* answer = "id accepted\n"; 
        send(client.socket, answer, strlen(answer), 0);
        std::cout << "Client " << client.id << " connected\n";
        break;
    }
    return client;
}

std::tuple<bool, std::string, std::string> validate_message(char buffer[], int bytes_received){
    if (bytes_received < PACKET_HEADER_SIZE) return {false, "d", ""};
    
    int content_size = 0;
    const auto [ptr, ec] = std::from_chars(buffer + 2, buffer + 6, content_size);
    if( ec !=  std::errc()){
        perror("content size conversion from packet header error");
        return {false, "", ""};
    }

    std::cout<<"DEBUG:\n";
    std::cout<<"CONTENT_SIZE:"<<content_size<<"\n";
    std::cout<<"BYTES_RECV:"<<bytes_received - PACKET_HEADER_SIZE<<"\n";


    if( content_size + 1 != bytes_received - PACKET_HEADER_SIZE){ //TODO: STWIERDZIC CZY CONTENT_SIZE Z ZNAKIEM KONCA TEKSTU?????
        return {false, "", ""};
    }

    std::string message_type(buffer,2);
    std::string content(buffer + 6);

    std::cout<<"MESSAGE_TYPE:"<<message_type<<"\n";
    std::cout<<"CONTENT:"<<content<<"\n";

    if (message_type == "SM" || message_type == "SS" || message_type == "SU" || message_type == "PC" || message_type == "PD" || message_type == "PB"){
        return {true, message_type, content};
        }
    else{
        return {false, "", ""};
    }
}