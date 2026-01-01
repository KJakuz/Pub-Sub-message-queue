#include "protocol_handler.h"
#include <sys/socket.h>
#include <iostream>
#include <charconv>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

std::string prepare_message(const std::string &message_type, const std::string &payload) {
    std::string buf;
    buf.reserve(PACKET_HEADER_SIZE + payload.size());
    buf += message_type;
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    buf.append(reinterpret_cast<const char *>(&len), sizeof(len));
    buf.append(payload);
    return buf;
}

static ssize_t recv_exact(int sock, char* buffer, size_t n) {
    size_t total_received = 0;
    while (total_received < n) {
        ssize_t received = recv(sock, buffer + total_received, n - total_received, 0);
        if (received <= 0) {
            //0 = disconnect, -1 = error
            return received;
        }
        total_received += received;
    }
    return static_cast<ssize_t>(total_received);
}


std::tuple<int, std::string, std::string> recv_message(int sock) {
    //status: succes = 1, disconnect = 0, error = -1
    char header[PACKET_HEADER_SIZE];
    ssize_t header_result = recv_exact(sock, header, PACKET_HEADER_SIZE);
    
    if (header_result == 0) return {0, "", ""};  
    if (header_result < 0) return {-1, "", ""};  
    
    std::string msg_type(header, 2);
    uint32_t network_len;
    std::memcpy(&network_len, header + 2, sizeof(uint32_t));
    uint32_t payload_size = ntohl(network_len);
    
    //100MB limit
    if (payload_size > 100 * 1024 * 1024) return {-1, "", ""};
    
    std::string msg_content;
    if (payload_size > 0) {
        msg_content.resize(payload_size);
        ssize_t payload_result = recv_exact(sock, msg_content.data(), payload_size);
        if (payload_result == 0) return {0, "", ""};
        if (payload_result < 0) return {-1, "", ""};
    }
    
    if (DEBUG == 1){
        safe_print("DEBUG: TYPE: " + msg_type + " SIZE: " + std::to_string(payload_size) + " CONTENT: " + msg_content);
    }
    
    if (msg_type == "LO" || msg_type == "SS" || msg_type == "SU" || 
        msg_type == "PC" || msg_type == "PD" || msg_type == "PB" || msg_type == "HB") {
        return {1, msg_type, msg_content};
    }
    return {-1, msg_type, msg_content};
}

bool send_message(int sock, const std::string &data) {
    if (sock == -1) return false;

    //sending to the same socket at the same time is not allowed valgrind gives errors
    static std::mutex socket_locks[1024];
    std::lock_guard<std::mutex> lock(socket_locks[sock % 1024]);

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