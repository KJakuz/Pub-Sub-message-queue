#include "MessageQueueClient.h"
#include "Protocol.h"
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <thread>

bool MessageQueueClient::connect_to_server(const std::string &host, const std::string &port, const int &client_id) {
    addrinfo hints{ .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, .ai_protocol = IPPROTO_TCP }, *res{};
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

    std::cout << "Connected: " << host.c_str() << ":" << port.c_str() << std::endl;
    _socket = sock;
    _connected = true;
    _receiver_thread = std::thread(&MessageQueueClient::_receiver_loop, this);
    return true;
}

void MessageQueueClient::disconnect() {
    if (_socket != -1) {
        if (_connected) {
            _receiver_thread.detach();
            shutdown(_socket, SHUT_RDWR);
        }
        close(_socket);
    }
}

bool MessageQueueClient::create_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["CREATE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::delete_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["DELETE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::publish(const std::string &queue_name, std::string &content) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["PUBLISH"];
    std::string internal_payload = Protocol::_pack_publish_data(queue_name, content, 3600);
    std::string message = Protocol::prepare_message(mode, action, internal_payload);
}

bool MessageQueueClient::subscribe(const std::string &queue_name) {
    char mode = ClientMode["SUBSCRIBER"];
    char action = ClientActions["SUBSCRIBE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::unsubscribe(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["UNSUBSCRIBE"];
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

void MessageQueueClient::_receiver_loop() {
    while(_connected) {
        char buf[1024];
        ssize_t bytes_read = recv(_socket, buf, 1024, 0);
        if (bytes_read > 0) {
            std::cout << "Bytes received from server: " << bytes_read << std::endl;
        }
    }
}

MessageQueueClient::MessageQueueClient() {}
MessageQueueClient::~MessageQueueClient() {
    disconnect();
}