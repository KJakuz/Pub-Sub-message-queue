#include "protocol_handler.h"
#include <sys/socket.h>
#include <iostream>
#include <charconv>

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

std::tuple<bool, std::string, std::string> validate_message(char buffer[], int bytes_received){
    if (bytes_received < PACKET_HEADER_SIZE) return {false, "", ""};
    
    int content_size = 0;
    const auto [ptr, ec] = std::from_chars(buffer + 2, buffer + 6, content_size);
    if( ec !=  std::errc()){
        perror("content size conversion from packet header error");
        return {false, "", ""};
    }

    std::cout<<"TEST:\n";
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