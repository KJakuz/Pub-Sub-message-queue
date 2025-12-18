#include "MessageQueueClient.h"
#include "Protocol.h"
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

bool MessageQueueClient::connect_to_server(const std::string &host, const std::string &port, const int &client_id) {
    addrinfo hints{ .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, .ai_protocol = IPPROTO_TCP }, *res;
    if (int err = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) {
        std::cerr << gai_strerror(err) << std::endl;
        return false;
    }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Error with connect" << std::endl;
        freeaddrinfo(res);
        close(sock);
        return false;
    }
    _socket = sock;
    return true;
}

void MessageQueueClient::disconnect() {
    if (_socket != -1) {
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
    }
}

bool MessageQueueClient::create_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    std::string message = Protocol::prepare_message(mode, 'c', queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::delete_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    std::string message = Protocol::prepare_message(mode, 'd', queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::publish(const std::string &queue_name, std::string &content) {
    // TODO: How to sent name and content?
}

bool MessageQueueClient::subscribe(const std::string &queue_name) {
    char mode = ClientMode["SUBSCRIBER"];
    std::string message = Protocol::prepare_message(mode, 's', queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::unsubscribe(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    std::string message = Protocol::prepare_message(mode, 'u', queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::send_message(int sock, const std::string &data) {
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