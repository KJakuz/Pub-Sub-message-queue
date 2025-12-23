#include "protocol_handler.h"
#include <sys/socket.h>
#include <iostream>
#include <charconv>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

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

    if (message_type == "LO" || message_type == "SS" || message_type == "SU" || message_type == "PC" || message_type == "PD" || message_type == "PB"){
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